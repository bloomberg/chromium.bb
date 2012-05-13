// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/update_recommended_message_box.h"

#include "chrome/browser/ui/browser_list.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// UpdateRecommendedMessageBox, public:

// static
void UpdateRecommendedMessageBox::Show(gfx::NativeWindow parent_window) {
  // When the window closes, it will delete itself.
  views::Widget::CreateWindowWithParent(new UpdateRecommendedMessageBox(),
                                        parent_window)->Show();
}

////////////////////////////////////////////////////////////////////////////////
// UpdateRecommendedMessageBox, private:

UpdateRecommendedMessageBox::UpdateRecommendedMessageBox() {
  const int kDialogWidth = 400;
#if defined(OS_CHROMEOS)
  const int kProductNameID = IDS_PRODUCT_OS_NAME;
#else
  const int kProductNameID = IDS_PRODUCT_NAME;
#endif
  const string16 product_name = l10n_util::GetStringUTF16(kProductNameID);
  views::MessageBoxView::InitParams params(
      l10n_util::GetStringFUTF16(IDS_UPDATE_RECOMMENDED, product_name));
  params.message_width = kDialogWidth;
  // Also deleted when the window closes.
  message_box_view_ = new views::MessageBoxView(params);
}

UpdateRecommendedMessageBox::~UpdateRecommendedMessageBox() {
}

bool UpdateRecommendedMessageBox::Accept() {
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
  // If running the Chrome OS build, but we're not on the device, fall through
#endif
  BrowserList::AttemptRestart();
  return true;
}

string16 UpdateRecommendedMessageBox::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return l10n_util::GetStringUTF16((button == ui::DIALOG_BUTTON_OK) ?
      IDS_RELAUNCH_AND_UPDATE : IDS_NOT_NOW);
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

ui::ModalType UpdateRecommendedMessageBox::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
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
