// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/gtk_chrome_cookie_view.h"

#include "base/i18n/time_formatting.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

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

GtkWidget* InitRowLabel(int row, int label_id, GtkWidget* details_table) {
  GtkWidget* name_label = gtk_label_new(
      l10n_util::GetStringUTF8(label_id).c_str());
  gtk_misc_set_alignment(GTK_MISC(name_label), 1, 0.5);
  gtk_table_attach(GTK_TABLE(details_table), name_label,
                   0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);

  return name_label;
}

GtkWidget* InitDetailRow(int row, int label_id,
                         GtkWidget* details_table, GtkWidget** entry) {
  GtkWidget* name_label = InitRowLabel(row, label_id, details_table);

  *entry = gtk_entry_new();
  gtk_entry_set_editable(GTK_ENTRY(*entry), FALSE);
  gtk_entry_set_has_frame(GTK_ENTRY(*entry), FALSE);
  gtk_table_attach_defaults(GTK_TABLE(details_table), *entry,
                            1, 2, row, row + 1);

  return name_label;
}

GtkWidget* InitComboboxRow(int row, int label_id,
                           GtkWidget* details_table,
                           GtkWidget** combobox,
                           GtkListStore** store) {
  GtkWidget* name_label = InitRowLabel(row, label_id, details_table);

  *store = gtk_list_store_new(1, G_TYPE_STRING);
  *combobox = gtk_combo_box_new_with_model(GTK_TREE_MODEL(*store));
  g_object_unref(*store);

  GtkCellRenderer* cell = gtk_cell_renderer_text_new();
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(*combobox), cell, TRUE);
  gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(*combobox), cell,
                                  "text", 0, NULL);

  GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), *combobox, FALSE, FALSE, 0);

  gtk_table_attach_defaults(GTK_TABLE(details_table), hbox,
                            1, 2, row, row + 1);

  return name_label;
}

void InitStyles(GtkChromeCookieView *self) {
  GtkStyle* label_style = gtk_widget_get_style(self->first_label_);
  GtkStyle* dialog_style = gtk_widget_get_style(GTK_WIDGET(self));

  // Cookie details.
  InitBrowserDetailStyle(self->cookie_name_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(self->cookie_content_entry_, label_style,
                         dialog_style);
  InitBrowserDetailStyle(self->cookie_domain_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(self->cookie_path_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(self->cookie_send_for_entry_, label_style,
                         dialog_style);
  InitBrowserDetailStyle(self->cookie_created_entry_, label_style,
                         dialog_style);
  if (self->cookie_expires_entry_) {
    InitBrowserDetailStyle(self->cookie_expires_entry_, label_style,
                           dialog_style);
  }

  // Database details.
  InitBrowserDetailStyle(self->database_name_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(self->database_description_entry_, label_style,
                         dialog_style);
  InitBrowserDetailStyle(self->database_size_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(self->database_last_modified_entry_, label_style,
                         dialog_style);

  // Local storage details.
  InitBrowserDetailStyle(self->local_storage_origin_entry_, label_style,
                         dialog_style);
  InitBrowserDetailStyle(self->local_storage_size_entry_, label_style,
                         dialog_style);
  InitBrowserDetailStyle(self->local_storage_last_modified_entry_, label_style,
                         dialog_style);

  // AppCache details.
  InitBrowserDetailStyle(self->appcache_manifest_entry_, label_style,
                         dialog_style);
  InitBrowserDetailStyle(self->appcache_size_entry_, label_style, dialog_style);
  InitBrowserDetailStyle(self->appcache_created_entry_, label_style,
                         dialog_style);
  InitBrowserDetailStyle(self->appcache_last_accessed_entry_, label_style,
                         dialog_style);

  // Local storage item.
  InitBrowserDetailStyle(self->local_storage_item_origin_entry_, label_style,
                         dialog_style);
  InitBrowserDetailStyle(self->local_storage_item_key_entry_, label_style,
                         dialog_style);
  InitBrowserDetailStyle(self->local_storage_item_value_entry_, label_style,
                         dialog_style);

  // Database accessed item.
  InitBrowserDetailStyle(self->database_accessed_origin_entry_, label_style,
                         dialog_style);
  InitBrowserDetailStyle(self->database_accessed_name_entry_, label_style,
                         dialog_style);
  InitBrowserDetailStyle(self->database_accessed_description_entry_,
                         label_style, dialog_style);
  InitBrowserDetailStyle(self->database_accessed_size_entry_, label_style,
                         dialog_style);

  // AppCache created item.
  InitBrowserDetailStyle(self->appcache_created_manifest_entry_, label_style,
                         dialog_style);
}

void SetCookieDetailsSensitivity(GtkChromeCookieView *self,
                                 gboolean enabled) {
  gtk_widget_set_sensitive(self->cookie_name_entry_, enabled);
  gtk_widget_set_sensitive(self->cookie_content_entry_, enabled);
  gtk_widget_set_sensitive(self->cookie_domain_entry_, enabled);
  gtk_widget_set_sensitive(self->cookie_path_entry_, enabled);
  gtk_widget_set_sensitive(self->cookie_send_for_entry_, enabled);
  gtk_widget_set_sensitive(self->cookie_created_entry_, enabled);
  if (self->cookie_expires_entry_)
    gtk_widget_set_sensitive(self->cookie_expires_entry_, enabled);
  else
    gtk_widget_set_sensitive(self->cookie_expires_combobox_, enabled);
}

void SetDatabaseDetailsSensitivity(GtkChromeCookieView *self,
                                   gboolean enabled) {
  gtk_widget_set_sensitive(self->database_name_entry_, enabled);
  gtk_widget_set_sensitive(self->database_description_entry_, enabled);
  gtk_widget_set_sensitive(self->database_size_entry_, enabled);
  gtk_widget_set_sensitive(self->database_last_modified_entry_, enabled);
}

void SetLocalStorageDetailsSensitivity(GtkChromeCookieView *self,
                                       gboolean enabled) {
  gtk_widget_set_sensitive(self->local_storage_origin_entry_, enabled);
  gtk_widget_set_sensitive(self->local_storage_size_entry_, enabled);
  gtk_widget_set_sensitive(self->local_storage_last_modified_entry_, enabled);
}

void SetAppCacheDetailsSensitivity(GtkChromeCookieView *self,
                                   gboolean enabled) {
  gtk_widget_set_sensitive(self->appcache_manifest_entry_, enabled);
  gtk_widget_set_sensitive(self->appcache_size_entry_, enabled);
  gtk_widget_set_sensitive(self->appcache_created_entry_, enabled);
  gtk_widget_set_sensitive(self->appcache_last_accessed_entry_, enabled);
}

void SetIndexedDBDetailsSensitivity(GtkChromeCookieView *self,
                                    gboolean enabled) {
  gtk_widget_set_sensitive(self->indexed_db_origin_entry_, enabled);
  gtk_widget_set_sensitive(self->indexed_db_size_entry_, enabled);
  gtk_widget_set_sensitive(self->indexed_db_last_modified_entry_, enabled);
}

void SetLocalStorageItemSensitivity(GtkChromeCookieView* self,
                                    gboolean enabled) {
  gtk_widget_set_sensitive(self->local_storage_item_origin_entry_, enabled);
  gtk_widget_set_sensitive(self->local_storage_item_key_entry_, enabled);
  gtk_widget_set_sensitive(self->local_storage_item_value_entry_, enabled);
}

void SetDatabaseAccessedSensitivity(GtkChromeCookieView* self,
                                    gboolean enabled) {
  gtk_widget_set_sensitive(self->database_accessed_origin_entry_, enabled);
  gtk_widget_set_sensitive(self->database_accessed_name_entry_, enabled);
  gtk_widget_set_sensitive(self->database_accessed_description_entry_, enabled);
  gtk_widget_set_sensitive(self->database_accessed_size_entry_, enabled);
}

void SetAppCacheCreatedSensitivity(GtkChromeCookieView* self,
                                   gboolean enabled) {
  gtk_widget_set_sensitive(self->appcache_created_manifest_entry_, enabled);
}

void ClearCookieDetails(GtkChromeCookieView *self) {
  std::string no_cookie =
      l10n_util::GetStringUTF8(IDS_COOKIES_COOKIE_NONESELECTED);
  gtk_entry_set_text(GTK_ENTRY(self->cookie_name_entry_),
                     no_cookie.c_str());
  gtk_entry_set_text(GTK_ENTRY(self->cookie_content_entry_),
                     no_cookie.c_str());
  gtk_entry_set_text(GTK_ENTRY(self->cookie_domain_entry_),
                     no_cookie.c_str());
  gtk_entry_set_text(GTK_ENTRY(self->cookie_path_entry_),
                     no_cookie.c_str());
  gtk_entry_set_text(GTK_ENTRY(self->cookie_created_entry_),
                     no_cookie.c_str());
  if (self->cookie_expires_entry_) {
    gtk_entry_set_text(GTK_ENTRY(self->cookie_expires_entry_),
                       no_cookie.c_str());
  } else {
    GtkListStore* store = self->cookie_expires_combobox_store_;
    GtkTreeIter iter;
    gtk_list_store_clear(store);

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, no_cookie.c_str(), -1);

    gtk_combo_box_set_active(GTK_COMBO_BOX(self->cookie_expires_combobox_),
                             0);
  }
  gtk_entry_set_text(GTK_ENTRY(self->cookie_send_for_entry_),
                     no_cookie.c_str());
  SetCookieDetailsSensitivity(self, FALSE);
}

void UpdateVisibleDetailedInfo(GtkChromeCookieView *self, GtkWidget* table) {
  SetCookieDetailsSensitivity(self, table == self->cookie_details_table_);
  SetDatabaseDetailsSensitivity(self, table == self->database_details_table_);
  SetLocalStorageDetailsSensitivity(self,
      table == self->local_storage_details_table_);
  SetAppCacheDetailsSensitivity(self, table == self->appcache_details_table_);
  SetIndexedDBDetailsSensitivity(self,
      table == self->indexed_db_details_table_);
  SetLocalStorageItemSensitivity(self,
      table == self->local_storage_item_table_);
  SetDatabaseAccessedSensitivity(self,
      table == self->database_accessed_table_);
  SetAppCacheCreatedSensitivity(self,
      table == self->appcache_created_table_);

  // Display everything
  gtk_widget_show(self->table_box_);
  gtk_widget_show_all(table);
  // Hide everything that isn't us.
  if (table != self->cookie_details_table_)
    gtk_widget_hide(self->cookie_details_table_);
  if (table != self->database_details_table_)
    gtk_widget_hide(self->database_details_table_);
  if (table != self->local_storage_details_table_)
    gtk_widget_hide(self->local_storage_details_table_);
  if (table != self->appcache_details_table_)
    gtk_widget_hide(self->appcache_details_table_);
  if (table != self->indexed_db_details_table_)
    gtk_widget_hide(self->indexed_db_details_table_);
  if (table != self->local_storage_item_table_)
    gtk_widget_hide(self->local_storage_item_table_);
  if (table != self->database_accessed_table_)
    gtk_widget_hide(self->database_accessed_table_);
  if (table != self->appcache_created_table_)
    gtk_widget_hide(self->appcache_created_table_);
}

}  // namespace

G_DEFINE_TYPE(GtkChromeCookieView, gtk_chrome_cookie_view, GTK_TYPE_FRAME)

static void gtk_chrome_cookie_view_class_init(GtkChromeCookieViewClass *klass) {
}

static void gtk_chrome_cookie_view_init(GtkChromeCookieView *self) {
}

void BuildWidgets(GtkChromeCookieView *self, gboolean editable_expiration) {
  self->table_box_ = gtk_vbox_new(FALSE, 0);
  gtk_widget_set_no_show_all(self->table_box_, TRUE);

  // Cookie details.
  self->cookie_details_table_ = gtk_table_new(7, 2, FALSE);
  gtk_container_add(GTK_CONTAINER(self->table_box_),
                    self->cookie_details_table_);
  gtk_table_set_col_spacing(GTK_TABLE(self->cookie_details_table_), 0,
                            gtk_util::kLabelSpacing);

  int row = 0;
  self->first_label_ = InitDetailRow(row++, IDS_COOKIES_COOKIE_NAME_LABEL,
                self->cookie_details_table_, &self->cookie_name_entry_);
  InitDetailRow(row++, IDS_COOKIES_COOKIE_CONTENT_LABEL,
                self->cookie_details_table_, &self->cookie_content_entry_);
  InitDetailRow(row++, IDS_COOKIES_COOKIE_DOMAIN_LABEL,
                self->cookie_details_table_, &self->cookie_domain_entry_);
  InitDetailRow(row++, IDS_COOKIES_COOKIE_PATH_LABEL,
                self->cookie_details_table_, &self->cookie_path_entry_);
  InitDetailRow(row++, IDS_COOKIES_COOKIE_SENDFOR_LABEL,
                self->cookie_details_table_, &self->cookie_send_for_entry_);
  InitDetailRow(row++, IDS_COOKIES_COOKIE_CREATED_LABEL,
                self->cookie_details_table_, &self->cookie_created_entry_);
  if (editable_expiration) {
    InitComboboxRow(row++, IDS_COOKIES_COOKIE_EXPIRES_LABEL,
                    self->cookie_details_table_,
                    &self->cookie_expires_combobox_,
                    &self->cookie_expires_combobox_store_);
  } else {
    InitDetailRow(row++, IDS_COOKIES_COOKIE_EXPIRES_LABEL,
                  self->cookie_details_table_, &self->cookie_expires_entry_);
  }

  // Database details.
  self->database_details_table_ = gtk_table_new(4, 2, FALSE);
  gtk_container_add(GTK_CONTAINER(self->table_box_),
                    self->database_details_table_);
  gtk_table_set_col_spacing(GTK_TABLE(self->database_details_table_), 0,
                            gtk_util::kLabelSpacing);

  InitDetailRow(row++, IDS_COOKIES_COOKIE_NAME_LABEL,
                self->database_details_table_, &self->database_name_entry_);
  InitDetailRow(row++, IDS_COOKIES_WEB_DATABASE_DESCRIPTION_LABEL,
                self->database_details_table_,
                &self->database_description_entry_);
  InitDetailRow(row++, IDS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL,
                self->database_details_table_, &self->database_size_entry_);
  InitDetailRow(row++, IDS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL,
                self->database_details_table_,
                &self->database_last_modified_entry_);

  // Local storage details.
  self->local_storage_details_table_ = gtk_table_new(3, 2, FALSE);
  gtk_container_add(GTK_CONTAINER(self->table_box_),
                    self->local_storage_details_table_);
  gtk_table_set_col_spacing(GTK_TABLE(self->local_storage_details_table_), 0,
                            gtk_util::kLabelSpacing);

  row = 0;
  InitDetailRow(row++, IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL,
                self->local_storage_details_table_,
                &self->local_storage_origin_entry_);
  InitDetailRow(row++, IDS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL,
                self->local_storage_details_table_,
                &self->local_storage_size_entry_);
  InitDetailRow(row++, IDS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL,
                self->local_storage_details_table_,
                &self->local_storage_last_modified_entry_);

  // AppCache details.
  self->appcache_details_table_ = gtk_table_new(4, 2, FALSE);
  gtk_container_add(GTK_CONTAINER(self->table_box_),
                    self->appcache_details_table_);
  gtk_table_set_col_spacing(GTK_TABLE(self->appcache_details_table_), 0,
                            gtk_util::kLabelSpacing);

  row = 0;
  InitDetailRow(row++, IDS_COOKIES_APPLICATION_CACHE_MANIFEST_LABEL,
                self->appcache_details_table_,
                &self->appcache_manifest_entry_);
  InitDetailRow(row++, IDS_COOKIES_SIZE_LABEL,
                self->appcache_details_table_, &self->appcache_size_entry_);
  InitDetailRow(row++, IDS_COOKIES_COOKIE_CREATED_LABEL,
                self->appcache_details_table_, &self->appcache_created_entry_);
  InitDetailRow(row++, IDS_COOKIES_LAST_ACCESSED_LABEL,
                self->appcache_details_table_,
                &self->appcache_last_accessed_entry_);

  // IndexedDB details.
  self->indexed_db_details_table_ = gtk_table_new(4, 2, FALSE);
  gtk_container_add(GTK_CONTAINER(self->table_box_),
                    self->indexed_db_details_table_);
  gtk_table_set_col_spacing(GTK_TABLE(self->indexed_db_details_table_), 0,
                            gtk_util::kLabelSpacing);

  row = 0;
  InitDetailRow(row++, IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL,
                self->indexed_db_details_table_,
                &self->indexed_db_origin_entry_);
  InitDetailRow(row++, IDS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL,
                self->indexed_db_details_table_, &self->indexed_db_size_entry_);
  InitDetailRow(row++, IDS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL,
                self->indexed_db_details_table_,
                &self->indexed_db_last_modified_entry_);

  // Local storage item.
  self->local_storage_item_table_ = gtk_table_new(3, 2, FALSE);
  gtk_container_add(GTK_CONTAINER(self->table_box_),
                    self->local_storage_item_table_);
  gtk_table_set_col_spacing(GTK_TABLE(self->local_storage_item_table_), 0,
                            gtk_util::kLabelSpacing);

  row = 0;
  InitDetailRow(row++, IDS_COOKIES_COOKIE_DOMAIN_LABEL,
                self->local_storage_item_table_,
                &self->local_storage_item_origin_entry_);
  InitDetailRow(row++, IDS_COOKIES_LOCAL_STORAGE_KEY_LABEL,
                self->local_storage_item_table_,
                &self->local_storage_item_key_entry_);
  InitDetailRow(row++, IDS_COOKIES_LOCAL_STORAGE_VALUE_LABEL,
                self->local_storage_item_table_,
                &self->local_storage_item_value_entry_);

  // Database accessed prompt.
  self->database_accessed_table_ = gtk_table_new(2, 2, FALSE);
  gtk_container_add(GTK_CONTAINER(self->table_box_),
                    self->database_accessed_table_);
  gtk_table_set_col_spacing(GTK_TABLE(self->local_storage_item_table_), 0,
                            gtk_util::kLabelSpacing);

  row = 0;
  InitDetailRow(row++, IDS_COOKIES_COOKIE_DOMAIN_LABEL,
                self->database_accessed_table_,
                &self->database_accessed_origin_entry_);
  InitDetailRow(row++, IDS_COOKIES_WEB_DATABASE_NAME,
                self->database_accessed_table_,
                &self->database_accessed_name_entry_);
  InitDetailRow(row++, IDS_COOKIES_WEB_DATABASE_DESCRIPTION_LABEL,
                self->database_accessed_table_,
                &self->database_accessed_description_entry_);
  InitDetailRow(row++, IDS_COOKIES_SIZE_LABEL,
                self->database_accessed_table_,
                &self->database_accessed_size_entry_);

  // AppCache created prompt.
  self->appcache_created_table_ = gtk_table_new(1, 2, FALSE);
  gtk_container_add(GTK_CONTAINER(self->table_box_),
                    self->appcache_created_table_);
  gtk_table_set_col_spacing(GTK_TABLE(self->appcache_created_table_), 0,
                            gtk_util::kLabelSpacing);
  row = 0;
  InitDetailRow(row++, IDS_COOKIES_APPLICATION_CACHE_MANIFEST_LABEL,
                self->appcache_created_table_,
                &self->appcache_created_manifest_entry_);

  gtk_frame_set_shadow_type(GTK_FRAME(self), GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(self), self->table_box_);
}

GtkWidget* gtk_chrome_cookie_view_new(gboolean editable_expiration) {
  GtkChromeCookieView* view = GTK_CHROME_COOKIE_VIEW(
      g_object_new(GTK_TYPE_CHROME_COOKIE_VIEW, NULL));
  BuildWidgets(view, editable_expiration);
  g_signal_connect(view, "realize", G_CALLBACK(InitStyles), NULL);
  return GTK_WIDGET(view);
}

void gtk_chrome_cookie_view_clear(GtkChromeCookieView* self) {
  UpdateVisibleDetailedInfo(self, self->cookie_details_table_);
  ClearCookieDetails(self);
}

// Switches the display to showing the passed in cookie.
void gtk_chrome_cookie_view_display_cookie(
    GtkChromeCookieView* self,
    const std::string& domain,
    const net::CookieMonster::CanonicalCookie& cookie) {
  UpdateVisibleDetailedInfo(self, self->cookie_details_table_);

  gtk_entry_set_text(GTK_ENTRY(self->cookie_name_entry_),
                     cookie.Name().c_str());
  gtk_entry_set_text(GTK_ENTRY(self->cookie_content_entry_),
                     cookie.Value().c_str());
  gtk_entry_set_text(GTK_ENTRY(self->cookie_domain_entry_),
                     domain.c_str());
  gtk_entry_set_text(GTK_ENTRY(self->cookie_path_entry_),
                     cookie.Path().c_str());
  gtk_entry_set_text(GTK_ENTRY(self->cookie_created_entry_),
                     UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                         cookie.CreationDate())).c_str());

  std::string expire_text = cookie.DoesExpire() ?
      UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(cookie.ExpiryDate())) :
      l10n_util::GetStringUTF8(IDS_COOKIES_COOKIE_EXPIRES_SESSION);

  if (self->cookie_expires_entry_) {
    gtk_entry_set_text(GTK_ENTRY(self->cookie_expires_entry_),
                       expire_text.c_str());
  } else {
    GtkListStore* store = self->cookie_expires_combobox_store_;
    GtkTreeIter iter;
    gtk_list_store_clear(store);

    if (cookie.DoesExpire()) {
      gtk_list_store_append(store, &iter);
      gtk_list_store_set(store, &iter, 0, expire_text.c_str(), -1);
    }

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(
        store, &iter, 0,
        l10n_util::GetStringUTF8(IDS_COOKIES_COOKIE_EXPIRES_SESSION).c_str(),
        -1);

    gtk_combo_box_set_active(GTK_COMBO_BOX(self->cookie_expires_combobox_),
                             0);
  }

  gtk_entry_set_text(
      GTK_ENTRY(self->cookie_send_for_entry_),
      l10n_util::GetStringUTF8(cookie.IsSecure() ?
                               IDS_COOKIES_COOKIE_SENDFOR_SECURE :
                               IDS_COOKIES_COOKIE_SENDFOR_ANY).c_str());
  SetCookieDetailsSensitivity(self, TRUE);
}

void gtk_chrome_cookie_view_display_cookie_string(
    GtkChromeCookieView* self,
    const GURL& url,
    const std::string& cookie_line) {
  net::CookieMonster::ParsedCookie pc(cookie_line);
  net::CookieMonster::CanonicalCookie cookie(url, pc);

  gtk_chrome_cookie_view_display_cookie(
      self,
      pc.HasDomain() ? pc.Domain() : url.host(),
      cookie);
}

// Switches the display to showing the passed in database.
void gtk_chrome_cookie_view_display_database(
    GtkChromeCookieView* self,
    const BrowsingDataDatabaseHelper::DatabaseInfo& database_info) {
  UpdateVisibleDetailedInfo(self, self->database_details_table_);

  gtk_entry_set_text(
      GTK_ENTRY(self->database_name_entry_),
      database_info.database_name.empty() ?
          l10n_util::GetStringUTF8(
              IDS_COOKIES_WEB_DATABASE_UNNAMED_NAME).c_str() :
          database_info.database_name.c_str());
  gtk_entry_set_text(GTK_ENTRY(self->database_description_entry_),
                     database_info.description.c_str());
  gtk_entry_set_text(GTK_ENTRY(self->database_size_entry_),
                     UTF16ToUTF8(FormatBytes(
                         database_info.size,
                         GetByteDisplayUnits(database_info.size),
                         true)).c_str());
  gtk_entry_set_text(GTK_ENTRY(self->database_last_modified_entry_),
                     UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                         database_info.last_modified)).c_str());
  SetDatabaseDetailsSensitivity(self, TRUE);
}

// Switches the display to showing the passed in local storage data.
void gtk_chrome_cookie_view_display_local_storage(
    GtkChromeCookieView* self,
    const BrowsingDataLocalStorageHelper::LocalStorageInfo&
    local_storage_info) {
  UpdateVisibleDetailedInfo(self, self->local_storage_details_table_);

  gtk_entry_set_text(GTK_ENTRY(self->local_storage_origin_entry_),
                     local_storage_info.origin.c_str());
  gtk_entry_set_text(GTK_ENTRY(self->local_storage_size_entry_),
                     UTF16ToUTF8(FormatBytes(
                         local_storage_info.size,
                         GetByteDisplayUnits(local_storage_info.size),
                         true)).c_str());
  gtk_entry_set_text(GTK_ENTRY(self->local_storage_last_modified_entry_),
                     UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                         local_storage_info.last_modified)).c_str());
  SetLocalStorageDetailsSensitivity(self, TRUE);
}

// Switches the display to showing the passed in app cache.
void gtk_chrome_cookie_view_display_app_cache(
    GtkChromeCookieView* self,
    const appcache::AppCacheInfo& info) {
  UpdateVisibleDetailedInfo(self, self->appcache_details_table_);

  gtk_entry_set_text(GTK_ENTRY(self->appcache_manifest_entry_),
                     info.manifest_url.spec().c_str());
  gtk_entry_set_text(GTK_ENTRY(self->appcache_size_entry_),
                     UTF16ToUTF8(FormatBytes(
                         info.size,
                         GetByteDisplayUnits(info.size),
                         true)).c_str());
  gtk_entry_set_text(GTK_ENTRY(self->appcache_created_entry_),
                     UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                         info.creation_time)).c_str());
  gtk_entry_set_text(GTK_ENTRY(self->appcache_last_accessed_entry_),
                     UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                         info.last_access_time)).c_str());
  SetAppCacheDetailsSensitivity(self, TRUE);
}

// Switches the display to showing the passed in IndexedDB data.
void gtk_chrome_cookie_view_display_indexed_db(
    GtkChromeCookieView* self,
    const BrowsingDataIndexedDBHelper::IndexedDBInfo& indexed_db_info) {
  UpdateVisibleDetailedInfo(self, self->indexed_db_details_table_);

  gtk_entry_set_text(GTK_ENTRY(self->indexed_db_origin_entry_),
                     indexed_db_info.origin.c_str());
  gtk_entry_set_text(GTK_ENTRY(self->indexed_db_size_entry_),
                     UTF16ToUTF8(FormatBytes(
                         indexed_db_info.size,
                         GetByteDisplayUnits(indexed_db_info.size),
                         true)).c_str());
  gtk_entry_set_text(GTK_ENTRY(self->indexed_db_last_modified_entry_),
                     UTF16ToUTF8(base::TimeFormatFriendlyDateAndTime(
                         indexed_db_info.last_modified)).c_str());
  SetLocalStorageDetailsSensitivity(self, TRUE);
}

void gtk_chrome_cookie_view_display_local_storage_item(
    GtkChromeCookieView* self,
    const std::string& host,
    const string16& key,
    const string16& value) {
  UpdateVisibleDetailedInfo(self, self->local_storage_item_table_);

  gtk_entry_set_text(GTK_ENTRY(self->local_storage_item_origin_entry_),
                     host.c_str());
  gtk_entry_set_text(GTK_ENTRY(self->local_storage_item_key_entry_),
                     UTF16ToUTF8(key).c_str());
  gtk_entry_set_text(GTK_ENTRY(self->local_storage_item_value_entry_),
                     UTF16ToUTF8(value).c_str());
  SetLocalStorageItemSensitivity(self, TRUE);
}

void gtk_chrome_cookie_view_display_database_accessed(
    GtkChromeCookieView* self,
    const std::string& host,
    const string16& database_name,
    const string16& display_name,
    unsigned long estimated_size) {
  UpdateVisibleDetailedInfo(self, self->database_accessed_table_);

  gtk_entry_set_text(GTK_ENTRY(self->database_accessed_origin_entry_),
                     host.c_str());
  gtk_entry_set_text(GTK_ENTRY(self->database_accessed_name_entry_),
                     UTF16ToUTF8(database_name).c_str());
  gtk_entry_set_text(GTK_ENTRY(self->database_accessed_description_entry_),
                     UTF16ToUTF8(display_name).c_str());
  gtk_entry_set_text(GTK_ENTRY(self->database_accessed_size_entry_),
                     UTF16ToUTF8(FormatBytes(
                         estimated_size,
                         GetByteDisplayUnits(estimated_size),
                         true)).c_str());
  SetDatabaseAccessedSensitivity(self, TRUE);
}

void gtk_chrome_cookie_view_display_appcache_created(
    GtkChromeCookieView* self,
    const GURL& manifest_url) {
  UpdateVisibleDetailedInfo(self, self->appcache_created_table_);
  gtk_entry_set_text(GTK_ENTRY(self->appcache_created_manifest_entry_),
                     manifest_url.spec().c_str());
  SetAppCacheCreatedSensitivity(self, TRUE);
}

bool gtk_chrome_cookie_view_session_expires(GtkChromeCookieView* self) {
  if (self->cookie_expires_entry_)
    return false;

  GtkListStore* store = self->cookie_expires_combobox_store_;
  int store_size = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL);
  if (store_size == 1)
    return false;

  DCHECK_EQ(2, store_size);

  int selected = gtk_combo_box_get_active(GTK_COMBO_BOX(
      self->cookie_expires_combobox_));
  return selected == 1;
}
