// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_OPTIONS_SIMPLE_CONTENT_EXCEPTIONS_WINDOW_H_
#define CHROME_BROWSER_UI_GTK_OPTIONS_SIMPLE_CONTENT_EXCEPTIONS_WINDOW_H_
#pragma once

#include <gtk/gtk.h>

#include "base/scoped_ptr.h"
#include "chrome/browser/remove_rows_table_model.h"
#include "chrome/browser/ui/gtk/gtk_tree.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "ui/base/gtk/gtk_signal.h"

class SimpleContentExceptionsWindow
    : public gtk_tree::TableAdapter::Delegate {
 public:
  // Takes ownership of |model|.
  static void ShowExceptionsWindow(GtkWindow* parent,
                                   RemoveRowsTableModel* model,
                                   int tile_message_id);

  virtual ~SimpleContentExceptionsWindow();

  // gtk_tree::TableAdapter::Delegate implementation:
  virtual void SetColumnValues(int row, GtkTreeIter* iter);
  virtual void OnAnyModelUpdateStart();
  virtual void OnAnyModelUpdate();

 private:
  // Takes ownership of |model|.
  SimpleContentExceptionsWindow(GtkWindow* parent,
                                RemoveRowsTableModel* model,
                                int title_message_id);

  // Updates which buttons are enabled.
  void UpdateButtonState();

  // Callbacks for the buttons.
  CHROMEGTK_CALLBACK_0(SimpleContentExceptionsWindow, void, Remove);
  CHROMEGTK_CALLBACK_0(SimpleContentExceptionsWindow, void, RemoveAll);

  CHROMEGTK_CALLBACK_0(SimpleContentExceptionsWindow, void,
                       OnWindowDestroy);
  CHROMEGTK_CALLBACK_0(SimpleContentExceptionsWindow, void,
                       OnTreeSelectionChanged);

  // The list presented in |treeview_|; a gobject instead of a C++ object.
  GtkListStore* list_store_;

  // The C++, views-ish, cross-platform model class that actually contains the
  // gold standard data.
  scoped_ptr<RemoveRowsTableModel> model_;

  // The adapter that ferries data back and forth between |model_| and
  // |list_store_| whenever either of them change.
  scoped_ptr<gtk_tree::TableAdapter> model_adapter_;

  // The exception window.
  GtkWidget* dialog_;

  // The treeview that presents the site/action pairs.
  GtkWidget* treeview_;

  // The current user selection from |treeview_|.
  GtkTreeSelection* treeview_selection_;

  // Whether to ignore selection changes. This is set during model updates,
  // when the list store may be inconsistent with the table model.
  bool ignore_selection_changes_;

  // Buttons.
  GtkWidget* remove_button_;
  GtkWidget* remove_all_button_;
};

#endif  // CHROME_BROWSER_UI_GTK_OPTIONS_SIMPLE_CONTENT_EXCEPTIONS_WINDOW_H_
