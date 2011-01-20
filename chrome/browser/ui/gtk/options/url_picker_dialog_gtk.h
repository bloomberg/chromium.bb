// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_OPTIONS_URL_PICKER_DIALOG_GTK_H_
#define CHROME_BROWSER_UI_GTK_OPTIONS_URL_PICKER_DIALOG_GTK_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/ui/gtk/gtk_tree.h"
#include "ui/base/gtk/gtk_signal.h"

class AccessibleWidgetHelper;
class GURL;
class Profile;
class PossibleURLModel;

class UrlPickerDialogGtk : public gtk_tree::TableAdapter::Delegate {
 public:
  typedef Callback1<const GURL&>::Type UrlPickerCallback;

  UrlPickerDialogGtk(UrlPickerCallback* callback,
                     Profile* profile,
                     GtkWindow* parent);

  ~UrlPickerDialogGtk();

  // gtk_tree::TableAdapter::Delegate implementation.
  virtual void SetColumnValues(int row, GtkTreeIter* iter);

 private:
  // Call the callback based on url entry.
  void AddURL();

  // Set sensitivity of buttons based on url entry state.
  void EnableControls();

  // Return the entry-formatted url for path in the sorted model.
  std::string GetURLForPath(GtkTreePath* path) const;

  // GTK sorting callbacks.
  static gint CompareTitle(GtkTreeModel* model, GtkTreeIter* a, GtkTreeIter* b,
                           gpointer window);
  static gint CompareURL(GtkTreeModel* model, GtkTreeIter* a, GtkTreeIter* b,
                         gpointer window);

  CHROMEGTK_CALLBACK_0(UrlPickerDialogGtk, void, OnUrlEntryChanged);
  CHROMEGTK_CALLBACK_2(UrlPickerDialogGtk, void, OnHistoryRowActivated,
                       GtkTreePath*, GtkTreeViewColumn*);
  CHROMEGTK_CALLBACK_1(UrlPickerDialogGtk, void, OnResponse, int);
  CHROMEGTK_CALLBACK_0(UrlPickerDialogGtk, void, OnWindowDestroy);

  // Callback for user selecting rows in recent history list.
  CHROMEG_CALLBACK_0(UrlPickerDialogGtk, void, OnHistorySelectionChanged,
                     GtkTreeSelection*)

  // The dialog window.
  GtkWidget* dialog_;

  // The text entry for manually adding an URL.
  GtkWidget* url_entry_;

  // The add button (we need a reference to it so we can de-activate it when the
  // |url_entry_| is empty.)
  GtkWidget* add_button_;

  // The recent history list.
  GtkWidget* history_tree_;
  GtkListStore* history_list_store_;
  GtkTreeModel* history_list_sort_;
  GtkTreeSelection* history_selection_;

  // Profile.
  Profile* profile_;

  // The table model.
  scoped_ptr<PossibleURLModel> url_table_model_;
  scoped_ptr<gtk_tree::TableAdapter> url_table_adapter_;

  // Called if the user selects an url.
  UrlPickerCallback* callback_;

  // Helper object to manage accessibility metadata.
  scoped_ptr<AccessibleWidgetHelper> accessible_widget_helper_;

  DISALLOW_COPY_AND_ASSIGN(UrlPickerDialogGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_OPTIONS_URL_PICKER_DIALOG_GTK_H_
