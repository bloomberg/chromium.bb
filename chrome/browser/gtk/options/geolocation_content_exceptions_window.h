// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_OPTIONS_GEOLOCATION_CONTENT_EXCEPTIONS_WINDOW_H_
#define CHROME_BROWSER_GTK_OPTIONS_GEOLOCATION_CONTENT_EXCEPTIONS_WINDOW_H_

#include <gtk/gtk.h>

#include <set>

#include "app/gtk_signal.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/geolocation/geolocation_content_settings_table_model.h"
#include "chrome/browser/gtk/gtk_tree.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"

class GeolocationContentSettingsMap;

class GeolocationContentExceptionsWindow
    : public gtk_tree::TableAdapter::Delegate {
 public:
  static void ShowExceptionsWindow(GtkWindow* parent,
                                   GeolocationContentSettingsMap* map);

  // gtk_tree::TableAdapter::Delegate implementation:
  virtual void SetColumnValues(int row, GtkTreeIter* iter);

 private:
  // Column ids for |list_store_|.
  enum {
    COL_HOSTNAME,
    COL_ACTION,
    COL_COUNT
  };

  GeolocationContentExceptionsWindow(GtkWindow* parent,
                                     GeolocationContentSettingsMap* map);

  // Updates which buttons are enabled.
  void UpdateButtonState();

  void GetSelectedRows(GeolocationContentSettingsTableModel::Rows* rows);

  // Callbacks for the buttons.
  CHROMEGTK_CALLBACK_0(GeolocationContentExceptionsWindow, void, Remove);
  CHROMEGTK_CALLBACK_0(GeolocationContentExceptionsWindow, void, RemoveAll);

  CHROMEGTK_CALLBACK_0(GeolocationContentExceptionsWindow, void,
                       OnWindowDestroy);
  CHROMEGTK_CALLBACK_0(GeolocationContentExceptionsWindow, void,
                       OnTreeSelectionChanged);

  // The list presented in |treeview_|; a gobject instead of a C++ object.
  GtkListStore* list_store_;

  // The C++, views-ish, cross-platform model class that actually contains the
  // gold standard data.
  scoped_ptr<GeolocationContentSettingsTableModel> model_;

  // The adapter that ferries data back and forth between |model_| and
  // |list_store_| whenever either of them change.
  scoped_ptr<gtk_tree::TableAdapter> model_adapter_;

  // The exception window.
  GtkWidget* dialog_;

  // The treeview that presents the site/action pairs.
  GtkWidget* treeview_;

  // The current user selection from |treeview_|.
  GtkTreeSelection* treeview_selection_;

  // Buttons.
  GtkWidget* remove_button_;
  GtkWidget* remove_all_button_;
};

#endif  // CHROME_BROWSER_GTK_OPTIONS_GEOLOCATION_CONTENT_EXCEPTIONS_WINDOW_H_
