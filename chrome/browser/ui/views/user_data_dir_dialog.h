// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A dialog box that tells the user that we can't write to the specified user
// data directory.  Provides the user a chance to pick a different directory.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_DATA_DIR_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_USER_DATA_DIR_DIALOG_H_
#pragma once

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/shell_dialogs.h"
#include "views/window/dialog_delegate.h"

class FilePath;

namespace views {
class MessageBoxView;
}

class UserDataDirDialog : public views::DialogDelegate,
                          public MessageLoopForUI::Dispatcher,
                          public SelectFileDialog::Listener {
 public:
  // Creates and runs a user data directory picker dialog.  The method blocks
  // while the dialog is showing.  If the user picks a directory, this method
  // returns the chosen directory. |user_data_dir| is the value of the
  // directory we were not able to use.
  static FilePath RunUserDataDirDialog(const FilePath& user_data_dir);
  virtual ~UserDataDirDialog();

  FilePath user_data_dir() const { return user_data_dir_; }

  // views::DialogDelegate methods:
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  // views::WidgetDelegate methods:
  virtual bool IsModal() const OVERRIDE { return false; }
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  // MessageLoop::Dispatcher method:
  virtual bool Dispatch(const MSG& msg) OVERRIDE;

  // SelectFileDialog::Listener methods:
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

 private:
  explicit UserDataDirDialog(const FilePath& user_data_dir);

  // Empty until the user picks a directory.
  FilePath user_data_dir_;

  views::MessageBoxView* message_box_view_;
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // Used to keep track of whether or not to block the message loop (still
  // waiting for the user to dismiss the dialog).
  bool is_blocking_;

  DISALLOW_COPY_AND_ASSIGN(UserDataDirDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_DATA_DIR_DIALOG_H_
