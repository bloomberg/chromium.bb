// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/message_center/web_notification_tray.h"

#include "chrome/browser/browser_process.h"
#include "ui/display/screen.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/message_center/views/desktop_popup_alignment_delegate.h"
#include "ui/message_center/views/message_popup_collection.h"

namespace message_center {

MessageCenterTrayDelegate* CreateMessageCenterTray() {
  return new WebNotificationTray();
}

WebNotificationTray::WebNotificationTray() {
  message_center_tray_.reset(
      new MessageCenterTray(this, g_browser_process->message_center()));
  alignment_delegate_.reset(new message_center::DesktopPopupAlignmentDelegate);
  popup_collection_.reset(new message_center::MessagePopupCollection(
      NULL, message_center(), message_center_tray_.get(),
      alignment_delegate_.get()));
}

WebNotificationTray::~WebNotificationTray() {
  // Reset this early so that delegated events during destruction don't cause
  // problems.
  popup_collection_.reset();
  message_center_tray_.reset();
}

message_center::MessageCenter* WebNotificationTray::message_center() {
  return message_center_tray_->message_center();
}

bool WebNotificationTray::ShowPopups() {
  alignment_delegate_->StartObserving(display::Screen::GetScreen());
  popup_collection_->DoUpdateIfPossible();
  return true;
}

void WebNotificationTray::HidePopups() {
  DCHECK(popup_collection_.get());
  popup_collection_->MarkAllPopupsShown();
}

bool WebNotificationTray::ShowMessageCenter() {
  // Message center not available on Windows/Linux.
  return false;
}

void WebNotificationTray::HideMessageCenter() {
}

bool WebNotificationTray::ShowNotifierSettings() {
  // Message center settings not available on Windows/Linux.
  return false;
}

bool WebNotificationTray::IsContextMenuEnabled() const {
  // It can always return true because the notifications are invisible if
  // the context menu shouldn't be enabled, such as in the lock screen.
  return true;
}

void WebNotificationTray::OnMessageCenterTrayChanged() {
}

MessageCenterTray* WebNotificationTray::GetMessageCenterTray() {
  return message_center_tray_.get();
}

}  // namespace message_center
