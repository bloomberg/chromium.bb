// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/restart_message_box.h"

#include "base/utf_string_conversions.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/message_box_flags.h"
#include "views/controls/message_box_view.h"
#include "views/window/window.h"

////////////////////////////////////////////////////////////////////////////////
// RestartMessageBox, public:

// static
void RestartMessageBox::ShowMessageBox(gfx::NativeWindow parent_window) {
  // When the window closes, it will delete itself.
  new RestartMessageBox(parent_window);
}

int RestartMessageBox::GetDialogButtons() const {
  return ui::MessageBoxFlags::DIALOGBUTTON_OK;
}

std::wstring RestartMessageBox::GetDialogButtonLabel(
    ui::MessageBoxFlags::DialogButton button) const {
  DCHECK(button == ui::MessageBoxFlags::DIALOGBUTTON_OK);
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_OK));
}

std::wstring RestartMessageBox::GetWindowTitle() const {
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
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

////////////////////////////////////////////////////////////////////////////////
// RestartMessageBox, private:

RestartMessageBox::RestartMessageBox(gfx::NativeWindow parent_window) {
  const int kDialogWidth = 400;
  // Also deleted when the window closes.
  message_box_view_ = new MessageBoxView(
      ui::MessageBoxFlags::kFlagHasMessage |
          ui::MessageBoxFlags::kFlagHasOKButton,
      UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_OPTIONS_RESTART_REQUIRED)).c_str(),
      std::wstring(),
      kDialogWidth);
  views::Window::CreateChromeWindow(parent_window, gfx::Rect(), this)->Show();
}

RestartMessageBox::~RestartMessageBox() {
}
