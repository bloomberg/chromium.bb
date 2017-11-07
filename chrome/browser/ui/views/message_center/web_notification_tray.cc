// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/message_center/web_notification_tray.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "ui/display/screen.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_tray_delegate.h"
#include "ui/message_center/views/desktop_popup_alignment_delegate.h"
#include "ui/message_center/views/message_popup_collection.h"

message_center::MessageCenterTrayDelegate* CreateMessageCenterTrayDelegate() {
  return new WebNotificationTray();
}

WebNotificationTray::WebNotificationTray() {
  message_center_tray_.reset(new message_center::MessageCenterTray(
      this, g_browser_process->message_center()));
  alignment_delegate_.reset(new message_center::DesktopPopupAlignmentDelegate);
  popup_collection_.reset(new message_center::MessagePopupCollection(
      message_center(), message_center_tray_.get(), alignment_delegate_.get()));
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

bool WebNotificationTray::ShowMessageCenter(bool show_by_click) {
  // Message center not available on Windows/Linux.
  return false;
}

void WebNotificationTray::HideMessageCenter() {
}

bool WebNotificationTray::ShowNotifierSettings() {
  // Message center settings not available on Windows/Linux.
  return false;
}

void WebNotificationTray::OnMessageCenterTrayChanged() {}

message_center::MessageCenterTray*
WebNotificationTray::GetMessageCenterTrayForTesting() {
  return message_center_tray_.get();
}
