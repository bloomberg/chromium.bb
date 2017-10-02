// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/notifications/message_center_tray_bridge.h"

#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#import "ui/message_center/cocoa/popup_collection.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_tray.h"

message_center::MessageCenterTrayDelegate* CreateMessageCenterTrayDelegate() {
  return new MessageCenterTrayBridge(g_browser_process->message_center());
}

MessageCenterTrayBridge::MessageCenterTrayBridge(
    message_center::MessageCenter* message_center)
    : message_center_(message_center),
      tray_(new message_center::MessageCenterTray(this, message_center)) {}

MessageCenterTrayBridge::~MessageCenterTrayBridge() {
}

void MessageCenterTrayBridge::OnMessageCenterTrayChanged() {
}

bool MessageCenterTrayBridge::ShowPopups() {
  popup_collection_.reset(
      [[MCPopupCollection alloc] initWithMessageCenter:message_center_]);
  return true;
}

void MessageCenterTrayBridge::HidePopups() {
  popup_collection_.reset();
}

bool MessageCenterTrayBridge::ShowMessageCenter(bool show_by_click) {
  return false;
}

void MessageCenterTrayBridge::HideMessageCenter() {
}

bool MessageCenterTrayBridge::ShowNotifierSettings() {
  return false;
}

bool MessageCenterTrayBridge::IsContextMenuEnabled() const {
  return false;
}

message_center::MessageCenterTray*
MessageCenterTrayBridge::GetMessageCenterTray() {
  return tray_.get();
}
