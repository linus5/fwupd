/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2018 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <efivar.h>

#include "fu-uefi-common.h"
#include "fwupd-common.h"

gboolean
fu_uefi_secure_boot_enabled (void)
{
	gint rc;
	gsize data_size = 0;
	guint32 attributes = 0;
	g_autofree guint8 *data = NULL;

	rc = efi_get_variable (efi_guid_global, "SecureBoot", &data, &data_size, &attributes);
	if (rc < 0)
		return FALSE;
	if (data_size >= 1 && data[0] & 1)
		return TRUE;
	return FALSE;
}

static gint
fu_uefi_strcmp_sort_cb (gconstpointer a, gconstpointer b)
{
	const gchar *stra = *((const gchar **) a);
	const gchar *strb = *((const gchar **) b);
	return g_strcmp0 (stra, strb);
}

GPtrArray *
fu_uefi_get_esrt_entry_paths (const gchar *esrt_path, GError **error)
{
	GPtrArray *entries = g_ptr_array_new_with_free_func (g_free);
	const gchar *fn;
	g_autofree gchar *esrt_entries = NULL;
	g_autoptr(GDir) dir = NULL;

	/* search ESRT */
	esrt_entries = g_build_filename (esrt_path, "entries", NULL);
	dir = g_dir_open (esrt_entries, 0, error);
	if (dir == NULL)
		return NULL;
	while ((fn = g_dir_read_name (dir)) != NULL)
		g_ptr_array_add (entries, g_build_filename (esrt_entries, fn, NULL));

	/* sort by name */
	g_ptr_array_sort (entries, fu_uefi_strcmp_sort_cb);
	return entries;
}

gchar *
fu_uefi_get_full_esp_path (const gchar *esp_mount)
{
	const gchar *os_release_id = NULL;
	g_autoptr(GError) error_local = NULL;
	g_autoptr(GHashTable) os_release = fwupd_get_os_release (&error_local);
	if (os_release != NULL) {
		os_release_id = g_hash_table_lookup (os_release, "ID");
	} else {
		g_debug ("failed to get ID: %s", error_local->message);
	}
	if (os_release_id == NULL)
		os_release_id = "unknown";
	return g_build_filename (esp_mount, "EFI", os_release_id, NULL);
}

guint64
fu_uefi_read_file_as_uint64 (const gchar *path, const gchar *attr_name)
{
	g_autofree gchar *data = NULL;
	g_autofree gchar *fn = g_build_filename (path, attr_name, NULL);
	if (!g_file_get_contents (fn, &data, NULL, NULL))
		return 0x0;
	if (g_str_has_prefix (data, "0x"))
		return g_ascii_strtoull (data + 2, NULL, 16);
	return g_ascii_strtoull (data, NULL, 10);
}

gboolean
fu_uefi_prefix_efi_errors (GError **error)
{
	g_autoptr(GString) str = g_string_new (NULL);
	gint rc = 1;
	for (gint i = 0; rc > 0; i++) {
		gchar *filename = NULL;
		gchar *function = NULL;
		gchar *message = NULL;
		gint line = 0;
		gint err = 0;

		rc = efi_error_get (i, &filename, &function, &line,
				    &message, &err);
		if (rc <= 0)
			break;
		g_string_append_printf (str, "{error #%d} %s:%d %s(): %s: %s\t",
					i, filename, line, function,
					message, strerror (err));
	}
	if (str->len > 1)
		g_string_truncate (str, str->len - 1);
	g_prefix_error (error, "%s: ", str->str);
	return FALSE;
}