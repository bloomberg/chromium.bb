// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/confirm_message_box_dialog.h"

#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/message_box_flags.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

// static
void ConfirmMessageBoxDialog::Run(gfx::NativeWindow parent,
                                  ConfirmMessageBoxObserver* observer,
                                  const std::wstring& message_text,
                                  const std::wstring& window_title) {
  DCHECK(observer);
  ConfirmMessageBoxDialog* dialog = new ConfirmMessageBoxDialog(observer,
      message_text, window_title);
  views::Window* window = views::Window::CreateChromeWindow(
      parent, gfx::Rect(), dialog);
  window->Show();
}

// static
void ConfirmMessageBoxDialog::RunWithCustomConfiguration(
    gfx::NativeWindow parent,
    ConfirmMessageBoxObserver* observer,
    const std::wstring& message_text,
    const std::wstring& window_title,
    const std::wstring& confirm_label,
    const std::wstring& reject_label,
    const gfx::Size& preferred_size) {
  DCHECK(observer);
  ConfirmMessageBoxDialog* dialog = new ConfirmMessageBoxDialog(observer,
      message_text, window_title);
  dialog->preferred_size_ = preferred_size;
  dialog->confirm_label_ = confirm_label;
  dialog->reject_label_ = reject_label;
  views::Window* window = views::Window::CreateChromeWindow(
      parent, gfx::Rect(), dialog);
  window->Show();
}

ConfirmMessageBoxDialog::ConfirmMessageBoxDialog(
    ConfirmMessageBoxObserver* observer, const std::wstring& message_text,
    const std::wstring& window_title)
    : observer_(observer),
      window_title_(window_title),
      preferred_size_(gfx::Size(views::Window::GetLocalizedContentsSize(
          IDS_CONFIRM_MESSAGE_BOX_DEFAULT_WIDTH_CHARS,
          IDS_CONFIRM_MESSAGE_BOX_DEFAULT_HEIGHT_LINES))),
      confirm_label_(UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL))),
      reject_label_(UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL))) {
  message_label_ = new views::Label(message_text);
  message_label_->SetMultiLine(true);
  message_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(message_label_);
}

int ConfirmMessageBoxDialog::GetDialogButtons() const {
  return ui::MessageBoxFlags::DIALOGBUTTON_OK |
         ui::MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

std::wstring ConfirmMessageBoxDialog::GetWindowTitle() const {
  return window_title_;
}

std::wstring ConfirmMessageBoxDialog::GetDialogButtonLabel(
    ui::MessageBoxFlags::DialogButton button) const {
  if (button == ui::MessageBoxFlags::DIALOGBUTTON_OK) {
    return confirm_label_;
  }
  if (button == ui::MessageBoxFlags::DIALOGBUTTON_CANCEL)
    return reject_label_;
  return DialogDelegate::GetDialogButtonLabel(button);
}

bool ConfirmMessageBoxDialog::Accept() {
  observer_->OnConfirmMessageAccept();
  return true;  // Dialog may now be closed.
}

bool ConfirmMessageBoxDialog::Cancel() {
  observer_->OnConfirmMessageCancel();
  return true;  // Dialog may now be closed.
}

void ConfirmMessageBoxDialog::Layout() {
  gfx::Size sz = message_label_->GetPreferredSize();
  message_label_->SetBounds(kPanelHorizMargin, kPanelVertMargin,
                            width() - 2 * kPanelHorizMargin,
                            sz.height());
}

gfx::Size ConfirmMessageBoxDialog::GetPreferredSize() {
  return preferred_size_;
}
