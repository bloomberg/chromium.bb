// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/cookie_display_gtk.h"

#include "app/l10n_util.h"
#include "base/i18n/time_formatting.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "grit/generated_resources.h"

namespace {

void InitBrowserDetailStyle(GtkWidget* entry, GtkStyle* label_style,
                            GtkStyle* dialog_style) {
  gtk_widget_modify_fg(entry, GTK_STATE_NORMAL,
                       &label_style->fg[GTK_STATE_NORMAL]);
  gtk_widget_modify_fg(entry, GTK_STATE_INSENSITIVE,
                       &label_style->fg[GTK_STATE_INSENSITIVE]);
  // GTK_NO_WINDOW widgets like GtkLabel don't draw their own background, so we
  // combine the normal or insensitive foreground of the label style with the
  // normal background of the window style to achieve the "normal label" and
  // "insensitive label" colors.
  gtk_widget_modify_base(entry, GTK_STATE_NORMAL,
                         &dialog_style->bg[GTK_STATE_NORMAL]);
  gtk_widget_modify_base(entry, GTK_STATE_INSENSITIVE,
                         &dialog_style->bg[GTK_STATE_NORMAL]);
}

}  // namespace

CookieDisplayGtk::CookieDisplayGtk(GtkStyle* label_style,
                                   GtkStyle* dialog_style) {
  table_box_ = gtk_vbox_new(FALSE, 0);

  // Cookie details.
  cookie_details_table_ = gtk_table_new(7, 2, FALSE);
  gtk_container_add(GTK_CONTAINER(table_box_), cookie_details_table_);
  gtk_table_set_col_spacing(GTK_TABLE(cookie_details_table_), 0,
                            gtk_util::kLabelSpacing);

  int row = 0;
  InitDetailRow(row++, IDS_COOKIES_COOKIE_NAME_LABEL,
                cookie_details_table_, &cookie_name_entry_);
  InitDetailRow(row++, IDS_COOKIES_COOKIE_CONTENT_LABEL,
                cookie_details_table_, &cookie_content_entry_);
  InitDetailRow(row++, IDS_COOKIES_COOKIE_DOMAIN_LABEL,
                cookie_details_table_, &cookie_domain_entry_);
  InitDetailRow(row++, IDS_COOKIES_COOKIE_PATH_LABEL,
                cookie_details_table_, &cookie_path_entry_);
  InitDetailRow(row++, IDS_COOKIES_COOKIE_SENDFOR_LABEL,
                cookie_details_table_, &cookie_send_for_entry_);
  InitDetailRow(row++, IDS_COOKIES_COOKIE_CREATED_LABEL,
                cookie_details_table_, &cookie_created_entry_);
  InitDetailRow(row++, IDS_COOKIES_COOKIE_EXPIRES_LABEL,
                cookie_details_table_, &cookie_expires_entry_);

  // Database details.
  database_details_table_ = gtk_table_new(4, 2, FALSE);
  gtk_container_add(GTK_CONTAINER(table_box_),
                    database_details_table_);
  gtk_table_set_col_spacing(GTK_TABLE(database_details_table_), 0,
                            gtk_util::kLabelSpacing);

  InitDetailRow(row++, IDS_COOKIES_COOKIE_NAME_LABEL,
                database_details_table_, &database_name_entry_);
  InitDetailRow(row++, IDS_COOKIES_WEB_DATABASE_DESCRIPTION_LABEL,
                database_details_table_, &database_description_entry_);
  InitDetailRow(row++, IDS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL,
                database_details_table_, &database_size_entry_);
  InitDetailRow(row++, IDS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL,
                database_details_table_,
                &database_last_modified_entry_);

  // Local storage details.
  local_storage_details_table_ = gtk_table_new(3, 2, FALSE);
  gtk_container_add(GTK_CONTAINER(table_box_), local_storage_details_table_);
  gtk_table_set_col_spacing(GTK_TABLE(local_storage_details_table_), 0,
                            gtk_util::kLabelSpacing);

  row = 0;
  InitDetailRow(row++, IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL,
                local_storage_details_table_, &local_storage_origin_entry_);
  InitDetailRow(row++, IDS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL,
                local_storage_details_table_, &local_storage_size_entry_);
  InitDetailRow(row++, IDS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL,
                local_storage_details_table_,
                &local_storage_last_modified_entry_);

  // AppCache details.
  appcache_details_table_ = gtk_table_new(4, 2, FALSE);
  gtk_container_add(GTK_CONTAINER(table_box_), appcache_details_table_);
  gtk_table_set_col_spacing(GTK_TABLE(appcache_details_table_), 0,
                            gtk_util::kLabelSpacing);

  row = 0;
  InitDetailRow(row++, IDS_COOKIES_APPLICATION_CACHE_MANIFEST_LABEL,
                appcache_details_table_, &appcache_manifest_entry_);
  InitDetailRow(row++, IDS_COOKIES_SIZE_LABEL,
                appcache_details_table_, &appcache_size_entry_);
  InitDetailRow(row++, IDS_COOKIES_COOKIE_CREATED_LABEL,
                appcache_details_table_, &appcache_created_entry_);
  InitDetailRow(row++, IDS_COOKIES_LAST_ACCESSED_LABEL,
                appcache_details_table_, &appcache_last_accessed_entry_);

  // Create our frame.
  top_frame_.Own(gtk_frame_new(NULL));
  gtk_frame_set_shadow_type(GTK_FRAME(top_frame_.get()), GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(top_frame_.get()), table_box_);

  ClearDisplay();
  InitStyles(label_style, dialog_style);
}

CookieDisplayGtk::~CookieDisplayGtk() {
  top_frame_.Destroy();
}

void CookieDisplayGtk::ClearDisplay() {
  UpdateVisibleDetailedInfo(cookie_details_table_);
  ClearCookieDetails();
}

void CookieDisplayGtk::DisplayCookieDetails(
    const std::string& domain,
    const net::CookieMonster::CanonicalCookie& cookie) {
  UpdateVisibleDetailedInfo(cookie_details_table_);

  gtk_entry_set_text(GTK_ENTRY(cookie_name_entry_), cookie.Name().c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_content_entry_), cookie.Value().c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_domain_entry_), domain.c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_path_entry_), cookie.Path().c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_created_entry_),
                     WideToUTF8(base::TimeFormatFriendlyDateAndTime(
                         cookie.CreationDate())).c_str());
  if (cookie.DoesExpire()) {
    gtk_entry_set_text(GTK_ENTRY(cookie_expires_entry_),
                       WideToUTF8(base::TimeFormatFriendlyDateAndTime(
                           cookie.ExpiryDate())).c_str());
  } else {
    gtk_entry_set_text(GTK_ENTRY(cookie_expires_entry_),
                       l10n_util::GetStringUTF8(
                           IDS_COOKIES_COOKIE_EXPIRES_SESSION).c_str());
  }
  gtk_entry_set_text(
      GTK_ENTRY(cookie_send_for_entry_),
      l10n_util::GetStringUTF8(cookie.IsSecure() ?
                               IDS_COOKIES_COOKIE_SENDFOR_SECURE :
                               IDS_COOKIES_COOKIE_SENDFOR_ANY).c_str());
  SetCookieDetailsSensitivity(TRUE);
}

void CookieDisplayGtk::DisplayDatabaseDetails(
    const BrowsingDataDatabaseHelper::DatabaseInfo& database_info) {
  UpdateVisibleDetailedInfo(database_details_table_);

  gtk_entry_set_text(
      GTK_ENTRY(database_name_entry_),
      database_info.database_name.empty() ?
          l10n_util::GetStringUTF8(
              IDS_COOKIES_WEB_DATABASE_UNNAMED_NAME).c_str() :
          database_info.database_name.c_str());
  gtk_entry_set_text(GTK_ENTRY(database_description_entry_),
                     database_info.description.c_str());
  gtk_entry_set_text(GTK_ENTRY(database_size_entry_),
                     WideToUTF8(FormatBytes(
                         database_info.size,
                         GetByteDisplayUnits(database_info.size),
                         true)).c_str());
  gtk_entry_set_text(GTK_ENTRY(database_last_modified_entry_),
                     WideToUTF8(base::TimeFormatFriendlyDateAndTime(
                         database_info.last_modified)).c_str());
  SetDatabaseDetailsSensitivity(TRUE);
}

void CookieDisplayGtk::DisplayLocalStorageDetails(
    const BrowsingDataLocalStorageHelper::LocalStorageInfo&
    local_storage_info) {
  UpdateVisibleDetailedInfo(local_storage_details_table_);

  gtk_entry_set_text(GTK_ENTRY(local_storage_origin_entry_),
                     local_storage_info.origin.c_str());
  gtk_entry_set_text(GTK_ENTRY(local_storage_size_entry_),
                     WideToUTF8(FormatBytes(
                         local_storage_info.size,
                         GetByteDisplayUnits(local_storage_info.size),
                         true)).c_str());
  gtk_entry_set_text(GTK_ENTRY(local_storage_last_modified_entry_),
                     WideToUTF8(base::TimeFormatFriendlyDateAndTime(
                         local_storage_info.last_modified)).c_str());
  SetLocalStorageDetailsSensitivity(TRUE);
}

void CookieDisplayGtk::DisplayAppCacheDetails(
    const BrowsingDataAppCacheHelper::AppCacheInfo& info) {
  UpdateVisibleDetailedInfo(appcache_details_table_);

  gtk_entry_set_text(GTK_ENTRY(appcache_manifest_entry_),
                     info.manifest_url.spec().c_str());
  gtk_entry_set_text(GTK_ENTRY(appcache_size_entry_),
                     WideToUTF8(FormatBytes(
                         info.size,
                         GetByteDisplayUnits(info.size),
                         true)).c_str());
  gtk_entry_set_text(GTK_ENTRY(appcache_created_entry_),
                     WideToUTF8(base::TimeFormatFriendlyDateAndTime(
                         info.creation_time)).c_str());
  gtk_entry_set_text(GTK_ENTRY(appcache_last_accessed_entry_),
                     WideToUTF8(base::TimeFormatFriendlyDateAndTime(
                         info.last_access_time)).c_str());
  SetAppCacheDetailsSensitivity(TRUE);
}

void CookieDisplayGtk::InitDetailRow(int row, int label_id,
                                GtkWidget* details_table, GtkWidget** entry) {
  GtkWidget* name_label = gtk_label_new(
      l10n_util::GetStringUTF8(label_id).c_str());
  gtk_misc_set_alignment(GTK_MISC(name_label), 1, 0.5);
  gtk_table_attach(GTK_TABLE(details_table), name_label,
                   0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);

  *entry = gtk_entry_new();

  gtk_entry_set_editable(GTK_ENTRY(*entry), FALSE);
  gtk_entry_set_has_frame(GTK_ENTRY(*entry), FALSE);
  gtk_table_attach_defaults(GTK_TABLE(details_table), *entry,
                            1, 2, row, row + 1);
}

void CookieDisplayGtk::InitStyles(GtkStyle* label_style,
                                  GtkStyle* dialog_style) {
  // Cookie details.
  InitBrowserDetailStyle(cookie_name_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(cookie_content_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(cookie_domain_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(cookie_path_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(cookie_send_for_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(cookie_created_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(cookie_expires_entry_, label_style, dialog_style);

  // Database details.
  InitBrowserDetailStyle(database_name_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(database_description_entry_, label_style,
                         dialog_style);
  InitBrowserDetailStyle(database_size_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(database_last_modified_entry_, label_style,
                         dialog_style);

  // Local storage details.
  InitBrowserDetailStyle(local_storage_origin_entry_, label_style,
                         dialog_style);
  InitBrowserDetailStyle(local_storage_size_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(local_storage_last_modified_entry_, label_style,
                         dialog_style);

  // AppCache details.
  InitBrowserDetailStyle(appcache_manifest_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(appcache_size_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(appcache_created_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(appcache_last_accessed_entry_, label_style,
                         dialog_style);
}

void CookieDisplayGtk::UpdateVisibleDetailedInfo(GtkWidget* table) {
  SetCookieDetailsSensitivity(table == cookie_details_table_);
  SetDatabaseDetailsSensitivity(table == database_details_table_);
  SetLocalStorageDetailsSensitivity(table == local_storage_details_table_);
  SetAppCacheDetailsSensitivity(table == appcache_details_table_);
  // Toggle the parent (the table frame) visibility and sensitivity.
  gtk_widget_show(table_box_);
  gtk_widget_show(table);
  // Toggle the other tables.
  if (table != cookie_details_table_)
    gtk_widget_hide(cookie_details_table_);
  if (table != database_details_table_)
    gtk_widget_hide(database_details_table_);
  if (table != local_storage_details_table_)
    gtk_widget_hide(local_storage_details_table_);
  if (table != appcache_details_table_)
    gtk_widget_hide(appcache_details_table_);
}


void CookieDisplayGtk::SetCookieDetailsSensitivity(gboolean enabled) {
  gtk_widget_set_sensitive(cookie_name_entry_, enabled);
  gtk_widget_set_sensitive(cookie_content_entry_, enabled);
  gtk_widget_set_sensitive(cookie_domain_entry_, enabled);
  gtk_widget_set_sensitive(cookie_path_entry_, enabled);
  gtk_widget_set_sensitive(cookie_send_for_entry_, enabled);
  gtk_widget_set_sensitive(cookie_created_entry_, enabled);
  gtk_widget_set_sensitive(cookie_expires_entry_, enabled);
}

void CookieDisplayGtk::SetDatabaseDetailsSensitivity(gboolean enabled) {
  gtk_widget_set_sensitive(database_name_entry_, enabled);
  gtk_widget_set_sensitive(database_description_entry_, enabled);
  gtk_widget_set_sensitive(database_size_entry_, enabled);
  gtk_widget_set_sensitive(database_last_modified_entry_, enabled);
}

void CookieDisplayGtk::SetLocalStorageDetailsSensitivity(gboolean enabled) {
  gtk_widget_set_sensitive(local_storage_origin_entry_, enabled);
  gtk_widget_set_sensitive(local_storage_size_entry_, enabled);
  gtk_widget_set_sensitive(local_storage_last_modified_entry_, enabled);
}

void CookieDisplayGtk::SetAppCacheDetailsSensitivity(gboolean enabled) {
  gtk_widget_set_sensitive(appcache_manifest_entry_, enabled);
  gtk_widget_set_sensitive(appcache_size_entry_, enabled);
  gtk_widget_set_sensitive(appcache_created_entry_, enabled);
  gtk_widget_set_sensitive(appcache_last_accessed_entry_, enabled);
}

void CookieDisplayGtk::ClearCookieDetails() {
  std::string no_cookie =
      l10n_util::GetStringUTF8(IDS_COOKIES_COOKIE_NONESELECTED);
  gtk_entry_set_text(GTK_ENTRY(cookie_name_entry_), no_cookie.c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_content_entry_), no_cookie.c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_domain_entry_), no_cookie.c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_path_entry_), no_cookie.c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_created_entry_), no_cookie.c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_expires_entry_), no_cookie.c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_send_for_entry_), no_cookie.c_str());
  SetCookieDetailsSensitivity(FALSE);
}
