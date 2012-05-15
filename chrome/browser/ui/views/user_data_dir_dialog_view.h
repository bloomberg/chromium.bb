// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_DATA_DIR_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_USER_DATA_DIR_DIALOG_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class MessageBoxView;
}

// A dialog box that tells the user that we can't write to the specified user
// data directory. Provides the user a chance to pick a different directory.
class UserDataDirDialogView : public views::DialogDelegate,
                              public MessageLoopForUI::Dispatcher,
                              public SelectFileDialog::Listener {
 public:
  explicit UserDataDirDialogView(const FilePath& user_data_dir);
  virtual ~UserDataDirDialogView();

  FilePath user_data_dir() const { return user_data_dir_; }

  // Overridden from views::DialogDelegate:
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual bool Cancel() OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;

  // Overridden from MessageLoopForUI::Dispatcher:
  virtual bool Dispatch(const base::NativeEvent& msg) OVERRIDE;

  // Overridden from SelectFileDialog::Listener:
  virtual void FileSelected(const FilePath& path,
                            int index,
                            void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

 private:
  // Empty until the user picks a directory.
  FilePath user_data_dir_;

  views::MessageBoxView* message_box_view_;
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // Used to keep track of whether or not to block the message loop (still
  // waiting for the user to dismiss the dialog).
  bool is_blocking_;

  DISALLOW_COPY_AND_ASSIGN(UserDataDirDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_DATA_DIR_DIALOG_VIEW_H_
