// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/restart_message_box.h"

#include "base/utf_string_conversions.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/message_box_flags.h"
#include "ui/views/widget/widget.h"
#include "views/controls/message_box_view.h"

////////////////////////////////////////////////////////////////////////////////
// RestartMessageBox, public:

// static
void RestartMessageBox::ShowMessageBox(gfx::NativeWindow parent_window) {
  // When the window closes, it will delete itself.
  new RestartMessageBox(parent_window);
}

int RestartMessageBox::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK;
}

string16 RestartMessageBox::GetDialogButtonLabel(
    ui::DialogButton button) const {
  DCHECK_EQ(button, ui::DIALOG_BUTTON_OK);
  return l10n_util::GetStringUTF16(IDS_OK);
}

string16 RestartMessageBox::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
}

void RestartMessageBox::DeleteDelegate() {
  delete this;
}

bool RestartMessageBox::IsModal() const {
  return true;
}

views::View* RestartMessageBox::GetContentsView() {
  return message_box_view_;
}

views::Widget* RestartMessageBox::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* RestartMessageBox::GetWidget() const {
  return message_box_view_->GetWidget();
}

////////////////////////////////////////////////////////////////////////////////
// RestartMessageBox, private:

RestartMessageBox::RestartMessageBox(gfx::NativeWindow parent_window) {
  const int kDialogWidth = 400;
  // Also deleted when the window closes.
  message_box_view_ = new views::MessageBoxView(
      ui::MessageBoxFlags::kFlagHasMessage |
          ui::MessageBoxFlags::kFlagHasOKButton,
      l10n_util::GetStringUTF16(IDS_OPTIONS_RELAUNCH_REQUIRED),
      string16(),
      kDialogWidth);
  views::Widget::CreateWindowWithParent(this, parent_window)->Show();
}

RestartMessageBox::~RestartMessageBox() {
}
