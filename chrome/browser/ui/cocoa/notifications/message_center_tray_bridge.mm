// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/notifications/message_center_tray_bridge.h"

#include "chrome/browser/browser_process.h"
#import "ui/message_center/cocoa/popup_collection.h"
#import "ui/message_center/message_center_tray.h"

namespace message_center {

MessageCenterTrayDelegate* CreateMessageCenterTray() {
  return new MessageCenterTrayBridge(g_browser_process->message_center());
}

}  // namespace message_center

MessageCenterTrayBridge::MessageCenterTrayBridge(
    message_center::MessageCenter* message_center)
    : message_center_(message_center),
      tray_(new message_center::MessageCenterTray(this, message_center)) {
}

MessageCenterTrayBridge::~MessageCenterTrayBridge() {
}

void MessageCenterTrayBridge::OnMessageCenterTrayChanged() {
  // TODO(rsesek): Implement tray UI.
}

bool MessageCenterTrayBridge::ShowPopups() {
  popup_collection_.reset(
      [[MCPopupCollection alloc] initWithMessageCenter:message_center_]);
  return true;
}

void MessageCenterTrayBridge::HidePopups() {
  popup_collection_.reset();
}

void MessageCenterTrayBridge::UpdatePopups() {
  // Nothing to do since the popup collection observes the MessageCenter
  // directly.
}

bool MessageCenterTrayBridge::ShowMessageCenter() {
  // TODO(rsesek): Implement tray UI.
  return false;
}

void MessageCenterTrayBridge::HideMessageCenter() {
  // TODO(rsesek): Implement tray UI.
}
