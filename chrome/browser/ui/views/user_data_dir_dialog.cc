// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/views/user_data_dir_dialog.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"

// static
FilePath UserDataDirDialog::RunUserDataDirDialog(
    const FilePath& user_data_dir) {
  // When the window closes, it will delete itself.
  UserDataDirDialog* dialog = new UserDataDirDialog(user_data_dir);
  MessageLoopForUI::current()->RunWithDispatcher(dialog);
  return dialog->user_data_dir();
}

UserDataDirDialog::UserDataDirDialog(const FilePath& user_data_dir)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          select_file_dialog_(SelectFileDialog::Create(this))),
      is_blocking_(true) {
  const int kDialogWidth = 400;
  views::MessageBoxView::InitParams params(
      l10n_util::GetStringFUTF16(IDS_CANT_WRITE_USER_DIRECTORY_SUMMARY,
                                 user_data_dir.LossyDisplayName()));
  params.message_width = kDialogWidth;
  message_box_view_ = new views::MessageBoxView(params);

  views::Widget::CreateWindow(this)->Show();
}

UserDataDirDialog::~UserDataDirDialog() {
  select_file_dialog_->ListenerDestroyed();
}

string16 UserDataDirDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  switch (button) {
    case ui::DIALOG_BUTTON_OK:
      return l10n_util::GetStringUTF16(
          IDS_CANT_WRITE_USER_DIRECTORY_CHOOSE_DIRECTORY_BUTTON);
    case ui::DIALOG_BUTTON_CANCEL:
      return l10n_util::GetStringUTF16(
          IDS_CANT_WRITE_USER_DIRECTORY_EXIT_BUTTON);
    default:
      NOTREACHED();
  }
  return string16();
}

string16 UserDataDirDialog::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_CANT_WRITE_USER_DIRECTORY_TITLE);
}

void UserDataDirDialog::DeleteDelegate() {
  delete this;
}

bool UserDataDirDialog::Accept() {
  // Directory picker
  std::wstring dialog_title = UTF16ToWide(l10n_util::GetStringUTF16(
      IDS_CANT_WRITE_USER_DIRECTORY_CHOOSE_DIRECTORY_BUTTON));
  HWND owning_hwnd =
      GetAncestor(message_box_view_->GetWidget()->GetNativeView(), GA_ROOT);
  select_file_dialog_->SelectFile(SelectFileDialog::SELECT_FOLDER,
                                  dialog_title, FilePath(), NULL, 0,
                                  std::wstring(), NULL, owning_hwnd, NULL);
  return false;
}

bool UserDataDirDialog::Cancel() {
  is_blocking_ = false;
  return true;
}

views::View* UserDataDirDialog::GetContentsView() {
  return message_box_view_;
}

views::Widget* UserDataDirDialog::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* UserDataDirDialog::GetWidget() const {
  return message_box_view_->GetWidget();
}

bool UserDataDirDialog::Dispatch(const base::NativeEvent& msg) {
  TranslateMessage(&msg);
  DispatchMessage(&msg);
  return is_blocking_;
}

void UserDataDirDialog::FileSelected(const FilePath& path,
                                     int index, void* params) {
  user_data_dir_ = path;
  is_blocking_ = false;
}

void UserDataDirDialog::FileSelectionCanceled(void* params) {
}
