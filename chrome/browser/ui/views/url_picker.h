// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_URL_PICKER_H_
#define CHROME_BROWSER_UI_VIEWS_URL_PICKER_H_
#pragma once

#include "views/controls/button/native_button.h"
#include "views/controls/table/table_view_observer.h"
#include "views/controls/textfield/textfield_controller.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

namespace views {
class Button;
class Label;
class TableView;
}

class PossibleURLModel;
class Profile;
class UrlPicker;

// UrlPicker delegate. Notified when the user accepts the entry.
class UrlPickerDelegate {
 public:
  virtual ~UrlPickerDelegate();

  virtual void AddBookmark(UrlPicker* dialog,
                           const std::wstring& title,
                           const GURL& url) = 0;
};

////////////////////////////////////////////////////////////////////////////////
//
// This class implements the dialog that let the user add a bookmark or page
// to the list of urls to open on startup.
// UrlPicker deletes itself when the dialog is closed.
//
////////////////////////////////////////////////////////////////////////////////
class UrlPicker : public views::View,
                  public views::DialogDelegate,
                  public views::TextfieldController,
                  public views::TableViewObserver {
 public:
  UrlPicker(UrlPickerDelegate* delegate, Profile* profile);
  virtual ~UrlPicker();

  // Show the dialog on the provided contents.
  virtual void Show(HWND parent);

  // Closes the dialog.
  void Close();

  // views::DialogDelegate:
  virtual std::wstring GetWindowTitle() const;
  virtual bool IsModal() const;
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual bool Accept();
  virtual int GetDefaultDialogButton() const;
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const;
  virtual views::View* GetContentsView();

  // views::TextFieldController:
  virtual void ContentsChanged(views::Textfield* sender,
                               const std::wstring& new_contents);
  virtual bool HandleKeyEvent(views::Textfield* sender,
                              const views::KeyEvent& key_event) {
    return false;
  }

  // views::View:
  virtual gfx::Size GetPreferredSize();
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

  // views::TableViewObserver:
  virtual void OnSelectionChanged();
  virtual void OnDoubleClick();

 private:
  // Modify the model from the user interface.
  void PerformModelChange();

  // Returns the URL the user has typed.
  GURL GetInputURL() const;

  // Profile.
  Profile* profile_;

  // URL Field.
  views::Textfield* url_field_;

  // The table model.
  scoped_ptr<PossibleURLModel> url_table_model_;

  // The table of visited urls.
  views::TableView* url_table_;

  // The delegate.
  UrlPickerDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(UrlPicker);
};

#endif  // CHROME_BROWSER_UI_VIEWS_URL_PICKER_H_
