// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/repost_form_warning_view.h"

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#include "views/controls/message_box_view.h"
#include "views/window/window.h"

namespace browser {

// Declared in browser_dialogs.h so others don't have to depend on our header.
void ShowRepostFormWarningDialog(gfx::NativeWindow parent_window,
                                 TabContents* tab_contents) {
  new RepostFormWarningView(parent_window, tab_contents);
}

}  // namespace browser

//////////////////////////////////////////////////////////////////////////////
// RepostFormWarningView, constructor & destructor:

RepostFormWarningView::RepostFormWarningView(
    gfx::NativeWindow parent_window,
    TabContents* tab_contents)
      : navigation_controller_(&tab_contents->controller()),
        message_box_view_(NULL) {
  message_box_view_ = new MessageBoxView(
      MessageBoxFlags::kIsConfirmMessageBox,
      l10n_util::GetString(IDS_HTTP_POST_WARNING),
      std::wstring());
  dialog_ = tab_contents->CreateConstrainedDialog(this);

  registrar_.Add(this, NotificationType::LOAD_START,
                 Source<NavigationController>(navigation_controller_));
  registrar_.Add(this, NotificationType::TAB_CLOSING,
                 Source<NavigationController>(navigation_controller_));
  registrar_.Add(this, NotificationType::RELOADING,
                 Source<NavigationController>(navigation_controller_));
}

RepostFormWarningView::~RepostFormWarningView() {
}

//////////////////////////////////////////////////////////////////////////////
// RepostFormWarningView, views::DialogDelegate implementation:

std::wstring RepostFormWarningView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_HTTP_POST_WARNING_TITLE);
}

std::wstring RepostFormWarningView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  if (button == MessageBoxFlags::DIALOGBUTTON_OK)
    return l10n_util::GetString(IDS_HTTP_POST_WARNING_RESEND);
  if (button == MessageBoxFlags::DIALOGBUTTON_CANCEL)
    return l10n_util::GetString(IDS_CANCEL);
  return std::wstring();
}

void RepostFormWarningView::DeleteDelegate() {
  delete this;
}

bool RepostFormWarningView::Cancel() {
  if (navigation_controller_) {
    navigation_controller_->CancelPendingReload();
    Close();
  }
  return true;
}

bool RepostFormWarningView::Accept() {
  if (navigation_controller_) {
    navigation_controller_->ContinuePendingReload();
    Close();
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// RepostFormWarningView, views::WindowDelegate implementation:

views::View* RepostFormWarningView::GetContentsView() {
  return message_box_view_;
}

///////////////////////////////////////////////////////////////////////////////
// RepostFormWarningView, private:

void RepostFormWarningView::Close() {
  navigation_controller_ = NULL;
  if (dialog_) {
    dialog_->CloseConstrainedWindow();
  }
}

void RepostFormWarningView::Observe(NotificationType type,
                                    const NotificationSource& source,
                                    const NotificationDetails& details) {
  // Close the dialog if we load a page (because reloading might not apply to
  // the same page anymore) or if the tab is closed, because then we won't have
  // a navigation controller anymore.
  if (window() && navigation_controller_ &&
      (type == NotificationType::LOAD_START ||
       type == NotificationType::TAB_CLOSING ||
       type == NotificationType::RELOADING)) {
    DCHECK_EQ(Source<NavigationController>(source).ptr(),
              navigation_controller_);
    navigation_controller_ = NULL;
    Close();
  }
}
