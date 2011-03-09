// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_OPTIONS_CONTENT_EXCEPTIONS_WINDOW_GTK_H_
#define CHROME_BROWSER_UI_GTK_OPTIONS_CONTENT_EXCEPTIONS_WINDOW_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/content_exceptions_table_model.h"
#include "chrome/browser/ui/gtk/gtk_tree.h"
#include "chrome/browser/ui/gtk/options/content_exception_editor.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "ui/base/gtk/gtk_signal.h"

class HostContentSettingsMap;

// Dialog that lists each of the exceptions to the current content policy, and
// has options for adding/editing/removing entries. Modal to parrent.
class ContentExceptionsWindowGtk : public gtk_tree::TableAdapter::Delegate,
                                   public ContentExceptionEditor::Delegate {
 public:
  static void ShowExceptionsWindow(GtkWindow* window,
                                   HostContentSettingsMap* map,
                                   HostContentSettingsMap* off_the_record_map,
                                   ContentSettingsType content_type);

  ~ContentExceptionsWindowGtk();

  // gtk_tree::TableAdapter::Delegate implementation:
  virtual void SetColumnValues(int row, GtkTreeIter* iter);

  // ContentExceptionEditor::Delegate implementation:
  virtual void AcceptExceptionEdit(
      const ContentSettingsPattern& pattern,
      ContentSetting setting,
      bool is_off_the_record,
      int index,
      bool is_new);

 private:
  // Column ids for |list_store_|.
  enum {
    COL_PATTERN,
    COL_ACTION,
    COL_OTR,
    COL_COUNT
  };

  ContentExceptionsWindowGtk(GtkWindow* parent,
                             HostContentSettingsMap* map,
                             HostContentSettingsMap* off_the_record_map,
                             ContentSettingsType type);

  // Updates which buttons are enabled.
  void UpdateButtonState();

  // Callbacks for the buttons.
  CHROMEGTK_CALLBACK_0(ContentExceptionsWindowGtk, void, Add);
  CHROMEGTK_CALLBACK_0(ContentExceptionsWindowGtk, void, Edit);
  CHROMEGTK_CALLBACK_0(ContentExceptionsWindowGtk, void, Remove);
  CHROMEGTK_CALLBACK_0(ContentExceptionsWindowGtk, void, RemoveAll);

  // Returns the title of the window (changes based on what ContentSettingsType
  // was set to in the constructor).
  std::string GetWindowTitle() const;

  // Gets the selected indicies in the two list stores. Indicies are returned
  // in <list_store_, sort_list_store_> order.
  void GetSelectedModelIndices(std::set<std::pair<int, int> >* indices);

  // GTK Callbacks
  CHROMEGTK_CALLBACK_2(ContentExceptionsWindowGtk, void,
                       OnTreeViewRowActivate, GtkTreePath*, GtkTreeViewColumn*);
  CHROMEGTK_CALLBACK_0(ContentExceptionsWindowGtk, void, OnWindowDestroy);
  CHROMEGTK_CALLBACK_0(ContentExceptionsWindowGtk, void,
                       OnTreeSelectionChanged);

  // The list presented in |treeview_|. Separate from |list_store_|, the list
  // that backs |sort_model_|. This separation comes so the user can sort the
  // data on screen without changing the underlying |list_store_|.
  GtkTreeModel* sort_list_store_;

  // The backing to |sort_list_store_|. Doesn't change when sorted.
  GtkListStore* list_store_;

  // The C++, views-ish, cross-platform model class that actually contains the
  // gold standard data.
  scoped_ptr<ContentExceptionsTableModel> model_;

  // True if we also show exceptions from an OTR profile.
  bool allow_off_the_record_;

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
  GtkWidget* edit_button_;
  GtkWidget* remove_button_;
  GtkWidget* remove_all_button_;

  friend class ContentExceptionsWindowGtkUnittest;
};

#endif  // CHROME_BROWSER_UI_GTK_OPTIONS_CONTENT_EXCEPTIONS_WINDOW_GTK_H_
