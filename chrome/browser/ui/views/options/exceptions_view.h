// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OPTIONS_EXCEPTIONS_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OPTIONS_EXCEPTIONS_VIEW_H_
#pragma once

#include <string>

#include "chrome/browser/content_exceptions_table_model.h"
#include "chrome/browser/ui/views/options/exception_editor_view.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/content_settings_types.h"
#include "views/controls/button/button.h"
#include "views/controls/table/table_view_observer.h"
#include "views/window/dialog_delegate.h"

class HostContentSettingsMap;

namespace views {
class NativeButton;
class TableView;
}

// ExceptionsView is responsible for showing the user the set of content
// exceptions for a specific type. The exceptions are shown in a table view
// by way of a ContentExceptionsTableModel. The user can add/edit/remove
// exceptions. Editing and creating new exceptions is done way of the
// ExceptionEditorView.
// Use the ShowExceptionsWindow method to create and show an ExceptionsView
// for a specific type. ExceptionsView is deleted when the window closes.
class ExceptionsView : public ExceptionEditorView::Delegate,
                       public views::View,
                       public views::ButtonListener,
                       public views::DialogDelegate,
                       public views::TableViewObserver {
 public:
  // Shows the Exceptions window.
  static void ShowExceptionsWindow(gfx::NativeWindow parent,
                                   HostContentSettingsMap* map,
                                   HostContentSettingsMap* off_the_record_map,
                                   ContentSettingsType content_type);

  virtual ~ExceptionsView();

  // TableViewObserver overrides:
  virtual void OnSelectionChanged();
  virtual void OnDoubleClick();
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

  // ExceptionEditorView::Delegate implementation.
  virtual void AcceptExceptionEdit(
      const ContentSettingsPattern& pattern,
      ContentSetting setting,
      bool is_off_the_record,
      int index,
      bool is_new);

 private:
  ExceptionsView(HostContentSettingsMap* map,
                 HostContentSettingsMap* off_the_record_map,
                 ContentSettingsType type);

  void Init();

  // Resets the enabled state of the buttons from the model.
  void UpdateButtonState();

  // Adds a new item.
  void Add();

  // Edits the selected item.
  void Edit();

  // Removes the selected item.
  void Remove();

  // Removes all.
  void RemoveAll();

  // The model displayed in the table.
  ContentExceptionsTableModel model_;

  // True if the user can also add incognito entries.
  bool allow_off_the_record_;

  views::TableView* table_;

  views::NativeButton* add_button_;
  views::NativeButton* edit_button_;
  views::NativeButton* remove_button_;
  views::NativeButton* remove_all_button_;

  DISALLOW_COPY_AND_ASSIGN(ExceptionsView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OPTIONS_EXCEPTIONS_VIEW_H_

