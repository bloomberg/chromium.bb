// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_GTK_CHROME_COOKIE_VIEW_H_
#define CHROME_BROWSER_UI_GTK_GTK_CHROME_COOKIE_VIEW_H_
#pragma once

#include <gtk/gtk.h>

#include <string>

#include "chrome/browser/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "net/base/cookie_monster.h"

class GURL;

G_BEGIN_DECLS

#define GTK_TYPE_CHROME_COOKIE_VIEW gtk_chrome_cookie_view_get_type()

#define GTK_CHROME_COOKIE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), \
  GTK_TYPE_CHROME_COOKIE_VIEW, GtkChromeCookieView))

#define GTK_CHROME_COOKIE_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), \
  GTK_TYPE_CHROME_COOKIE_VIEW, GtkChromeCookieViewClass))

#define GTK_IS_CHROME_COOKIE_VIEW(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
  GTK_TYPE_CHROME_COOKIE_VIEW))

#define GTK_IS_CHROME_COOKIE_VIEW_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), \
  GTK_TYPE_CHROME_COOKIE_VIEW))

#define GTK_CHROME_COOKIE_VIEW_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), \
  GTK_TYPE_CHROME_COOKIE_VIEW, GtkChromeCookieViewClass))

// TODO(erg): Refactor the following class. It's continuously grown as more
// things have been added to it and should probably become a general key/value
// table. The problem is that any implementation for that would be much more
// complicated and would require changing a whole lot of code.
typedef struct {
  GtkFrame parent;

  // All public for testing since I don't think there's a "friend" mechanism in
  // gobject.

  GtkWidget* table_box_;

  // A label we keep around so we can access its GtkStyle* once it is realized.
  GtkWidget* first_label_;

  // The cookie details widgets.
  GtkWidget* cookie_details_table_;
  GtkWidget* cookie_name_entry_;
  GtkWidget* cookie_content_entry_;
  GtkWidget* cookie_domain_entry_;
  GtkWidget* cookie_path_entry_;
  GtkWidget* cookie_send_for_entry_;
  GtkWidget* cookie_created_entry_;

  // Note: These two widgets are mutually exclusive based on what
  // |editable_expiration| was when the cookie view was created. One of these
  // variables will be NULL.
  GtkWidget* cookie_expires_entry_;
  GtkWidget* cookie_expires_combobox_;

  GtkListStore* cookie_expires_combobox_store_;

  // The database details widgets.
  GtkWidget* database_details_table_;
  GtkWidget* database_name_entry_;
  GtkWidget* database_description_entry_;
  GtkWidget* database_size_entry_;
  GtkWidget* database_last_modified_entry_;

  // The local storage details widgets.
  GtkWidget* local_storage_details_table_;
  GtkWidget* local_storage_origin_entry_;
  GtkWidget* local_storage_size_entry_;
  GtkWidget* local_storage_last_modified_entry_;

  // The appcache details widgets.
  GtkWidget* appcache_details_table_;
  GtkWidget* appcache_manifest_entry_;
  GtkWidget* appcache_size_entry_;
  GtkWidget* appcache_created_entry_;
  GtkWidget* appcache_last_accessed_entry_;

  // The IndexedDB details widgets.
  GtkWidget* indexed_db_details_table_;
  GtkWidget* indexed_db_origin_entry_;
  GtkWidget* indexed_db_size_entry_;
  GtkWidget* indexed_db_last_modified_entry_;

  // The local storage item widgets.
  GtkWidget* local_storage_item_table_;
  GtkWidget* local_storage_item_origin_entry_;
  GtkWidget* local_storage_item_key_entry_;
  GtkWidget* local_storage_item_value_entry_;

  // The database accessed widgets.
  GtkWidget* database_accessed_table_;
  GtkWidget* database_accessed_origin_entry_;
  GtkWidget* database_accessed_name_entry_;
  GtkWidget* database_accessed_description_entry_;
  GtkWidget* database_accessed_size_entry_;

  // The appcache created widgets.
  GtkWidget* appcache_created_table_;
  GtkWidget* appcache_created_manifest_entry_;
} GtkChromeCookieView;

typedef struct {
  GtkFrameClass parent_class;
} GtkChromeCookieViewClass;

GType gtk_chrome_cookie_view_get_type();

// Builds a new cookie view.
GtkWidget* gtk_chrome_cookie_view_new(gboolean editable_expiration);

// Clears the cookie view.
void gtk_chrome_cookie_view_clear(GtkChromeCookieView* widget);

// NOTE: The G_END_DECLS ends here instead of at the end of the document
// because we want to define some methods on GtkChromeCookieView that take C++
// objects.
G_END_DECLS
// NOTE: ^^^^^^^^^^^^^^^^^^^^^^^

// Switches the display to showing the passed in cookie.
void gtk_chrome_cookie_view_display_cookie(
    GtkChromeCookieView* widget,
    const std::string& domain,
    const net::CookieMonster::CanonicalCookie& cookie);

// Looks up the cookie_line in CookieMonster and displays that.
void gtk_chrome_cookie_view_display_cookie_string(
    GtkChromeCookieView* widget,
    const GURL& url,
    const std::string& cookie_line);

// Switches the display to showing the passed in database.
void gtk_chrome_cookie_view_display_database(
    GtkChromeCookieView* widget,
    const BrowsingDataDatabaseHelper::DatabaseInfo& database_info);

// Switches the display to showing the passed in local storage data.
void gtk_chrome_cookie_view_display_local_storage(
    GtkChromeCookieView* widget,
    const BrowsingDataLocalStorageHelper::LocalStorageInfo&
    local_storage_info);

// Switches the display to showing the passed in app cache.
void gtk_chrome_cookie_view_display_app_cache(
    GtkChromeCookieView* widget,
    const appcache::AppCacheInfo& info);

// Switches the display to showing the passed in IndexedDB data.
void gtk_chrome_cookie_view_display_indexed_db(
    GtkChromeCookieView* widget,
    const BrowsingDataIndexedDBHelper::IndexedDBInfo& info);

// Switches the display to an individual storage item.
void gtk_chrome_cookie_view_display_local_storage_item(
    GtkChromeCookieView* widget,
    const std::string& host,
    const string16& key,
    const string16& value);

void gtk_chrome_cookie_view_display_database_accessed(
    GtkChromeCookieView* self,
    const std::string& host,
    const string16& database_name,
    const string16& display_name,
    unsigned long estimated_size);

void gtk_chrome_cookie_view_display_appcache_created(
    GtkChromeCookieView* self,
    const GURL& manifest_url);

// If |editable_expiration| was true at construction time, returns the value of
// the combo box. Otherwise, returns false.
bool gtk_chrome_cookie_view_session_expires(GtkChromeCookieView* self);

#endif  // CHROME_BROWSER_UI_GTK_GTK_CHROME_COOKIE_VIEW_H_
