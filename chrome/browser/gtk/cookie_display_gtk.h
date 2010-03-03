// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_COOKIE_DISPLAY_GTK_H_
#define CHROME_BROWSER_GTK_COOKIE_DISPLAY_GTK_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data_local_storage_helper.h"
#include "chrome/common/owned_widget_gtk.h"
#include "net/base/cookie_monster.h"

class CookieDisplayGtk {
 public:
  // Builds the cookie display. The display will blend |label_style| and
  // |dialog_style| to form the
  CookieDisplayGtk(GtkStyle* label_style, GtkStyle* dialog_style);
  virtual ~CookieDisplayGtk();

  GtkWidget* widget() { return top_frame_.get(); }

  void ClearDisplay();

  // Switches the display to showing the passed in cookie.
  void DisplayCookieDetails(const std::string& domain,
                            const net::CookieMonster::CanonicalCookie& cookie);

  // Switches the display to showing |database_info|.
  void DisplayDatabaseDetails(
      const BrowsingDataDatabaseHelper::DatabaseInfo& database_info);

  void DisplayLocalStorageDetails(
      const BrowsingDataLocalStorageHelper::LocalStorageInfo&
      local_storage_info);

  void DisplayAppCacheDetails(
      const BrowsingDataAppCacheHelper::AppCacheInfo& info);

 private:
  // Helper for initializing cookie / local storage details table.
  void InitDetailRow(int row, int label_id,
                     GtkWidget* details_table, GtkWidget** display_label);

  // Sets the style for each label.
  void InitStyles(GtkStyle* label_style, GtkStyle* dialog_style);

  // Sets which of the detailed info table is visible.
  void UpdateVisibleDetailedInfo(GtkWidget* table);

  // Set sensitivity of cookie details.
  void SetCookieDetailsSensitivity(gboolean enabled);

  // Set sensitivity of database details.
  void SetDatabaseDetailsSensitivity(gboolean enabled);

  // Set sensitivity of local storage details.
  void SetLocalStorageDetailsSensitivity(gboolean enabled);

  // Set sensitivity of appcache details.
  void SetAppCacheDetailsSensitivity(gboolean enabled);

  // Reset the cookie details display.
  void ClearCookieDetails();

  // The top level frame that we return in widget().
  OwnedWidgetGtk top_frame_;

  // An hbox which contains all the different tables we can show.
  GtkWidget* table_box_;

  // The cookie details widgets.
  GtkWidget* cookie_details_table_;
  GtkWidget* cookie_name_entry_;
  GtkWidget* cookie_content_entry_;
  GtkWidget* cookie_domain_entry_;
  GtkWidget* cookie_path_entry_;
  GtkWidget* cookie_send_for_entry_;
  GtkWidget* cookie_created_entry_;
  GtkWidget* cookie_expires_entry_;

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

  friend class CookiesViewTest;

  DISALLOW_COPY_AND_ASSIGN(CookieDisplayGtk);
};

#endif  // CHROME_BROWSER_GTK_COOKIE_DISPLAY_GTK_H_
