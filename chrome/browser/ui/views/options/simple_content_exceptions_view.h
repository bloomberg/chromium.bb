// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OPTIONS_SIMPLE_CONTENT_EXCEPTIONS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OPTIONS_SIMPLE_CONTENT_EXCEPTIONS_VIEW_H_
#pragma once

#include <string>

#include "chrome/browser/remove_rows_table_model.h"
#include "chrome/common/content_settings.h"
#include "views/controls/button/button.h"
#include "views/controls/table/table_view_observer.h"
#include "views/window/dialog_delegate.h"

namespace views {
class NativeButton;
class TableView;
}

// SimpleContentExceptionsView is responsible for showing the user the set of
// site-specific permissions. The exceptions are shown in a table view by way
// of a RemoveRowsTableModel. The user can remove exceptions.
// Use the ShowExceptionsWindow method to create and show a
// SimpleContentExceptionsView, which is deleted when the window closes.
class SimpleContentExceptionsView : public views::View,
                                    public views::ButtonListener,
                                    public views::DialogDelegate,
                                    public views::TableViewObserver {
 public:
  // Shows the Exceptions window. Takes ownership of |model|.
  static void ShowExceptionsWindow(gfx::NativeWindow parent,
                                   RemoveRowsTableModel* model,
                                   int title_message_id);

  virtual ~SimpleContentExceptionsView();

  // TableViewObserver overrides:
  virtual void OnSelectionChanged();
  virtual void OnTableViewDelete(views::TableView* table_view);

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // views::View overrides:
  virtual void Layout();
  virtual gfx::Size GetPreferredSize();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

  // views::WindowDelegate implementation.
  virtual int GetDialogButtons() const {
    return MessageBoxFlags::DIALOGBUTTON_CANCEL;
  }
  virtual bool CanResize() const { return true; }
  virtual std::wstring GetWindowTitle() const;
  virtual views::View* GetContentsView() { return this; }

 private:
  // Takes ownership of |model|.
  explicit SimpleContentExceptionsView(RemoveRowsTableModel* model,
                                       int title_message_id);

  void Init();

  // Resets the enabled state of the buttons from the model.
  void UpdateButtonState();

  // Returns the set of selected rows.
  RemoveRowsTableModel::Rows GetSelectedRows() const;

  // Removes the selected item.
  void Remove();

  // Removes all.
  void RemoveAll();

  // The model displayed in the table.
  scoped_ptr<RemoveRowsTableModel> model_;

  views::TableView* table_;

  views::NativeButton* remove_button_;
  views::NativeButton* remove_all_button_;

  // The message id of the window title.
  int title_message_id_;

  DISALLOW_COPY_AND_ASSIGN(SimpleContentExceptionsView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OPTIONS_SIMPLE_CONTENT_EXCEPTIONS_VIEW_H_

