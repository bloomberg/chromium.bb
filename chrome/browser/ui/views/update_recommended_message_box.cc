// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/update_recommended_message_box.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/window.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/message_box_flags.h"
#include "views/controls/message_box_view.h"
#include "views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// UpdateRecommendedMessageBox, public:

// static
void UpdateRecommendedMessageBox::ShowMessageBox(
    gfx::NativeWindow parent_window) {
  // When the window closes, it will delete itself.
  new UpdateRecommendedMessageBox(parent_window);
}

bool UpdateRecommendedMessageBox::Accept() {
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
  // If running the Chrome OS build, but we're not on the device, fall through
#endif
  BrowserList::AttemptRestart();
  return true;
}

int UpdateRecommendedMessageBox::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

string16 UpdateRecommendedMessageBox::GetDialogButtonLabel(
    ui::DialogButton button) const {
  DCHECK(button == ui::DIALOG_BUTTON_OK || button == ui::DIALOG_BUTTON_CANCEL);
  return button == ui::DIALOG_BUTTON_OK ?
      l10n_util::GetStringUTF16(IDS_RELAUNCH_AND_UPDATE) :
      l10n_util::GetStringUTF16(IDS_NOT_NOW);
}

bool UpdateRecommendedMessageBox::ShouldShowWindowTitle() const {
#if defined(OS_CHROMEOS)
  return false;
#else
  return true;
#endif
}

string16 UpdateRecommendedMessageBox::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
}

void UpdateRecommendedMessageBox::DeleteDelegate() {
  delete this;
}

bool UpdateRecommendedMessageBox::IsModal() const {
  return true;
}

views::View* UpdateRecommendedMessageBox::GetContentsView() {
  return message_box_view_;
}

views::Widget* UpdateRecommendedMessageBox::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* UpdateRecommendedMessageBox::GetWidget() const {
  return message_box_view_->GetWidget();
}

////////////////////////////////////////////////////////////////////////////////
// UpdateRecommendedMessageBox, private:

UpdateRecommendedMessageBox::UpdateRecommendedMessageBox(
    gfx::NativeWindow parent_window) {
  const int kDialogWidth = 400;
#if defined(OS_CHROMEOS)
  const int kProductNameId = IDS_PRODUCT_OS_NAME;
#else
  const int kProductNameId = IDS_PRODUCT_NAME;
#endif
  const string16 product_name = l10n_util::GetStringUTF16(kProductNameId);
  // Also deleted when the window closes.
  message_box_view_ = new views::MessageBoxView(
      ui::MessageBoxFlags::kFlagHasMessage |
          ui::MessageBoxFlags::kFlagHasOKButton,
      l10n_util::GetStringFUTF16(IDS_UPDATE_RECOMMENDED, product_name),
      string16(),
      kDialogWidth);
  browser::CreateViewsWindow(parent_window, this)->Show();
}

UpdateRecommendedMessageBox::~UpdateRecommendedMessageBox() {
}
