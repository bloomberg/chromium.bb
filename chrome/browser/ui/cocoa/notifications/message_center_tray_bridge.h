// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_MESSAGE_CENTER_TRAY_BRIDGE_H_
#define CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_MESSAGE_CENTER_TRAY_BRIDGE_H_

#import <AppKit/AppKit.h>

#include <memory>

#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_member.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_tray_delegate.h"

@class MCPopupCollection;

namespace message_center {
class MessageCenter;
class MessageCenterTray;
}

// MessageCenterTrayBridge is the owner of all the Cocoa UI objects for the
// message_center. It bridges C++ notifications from the MessageCenterTray to
// the various UI objects.
class MessageCenterTrayBridge :
    public message_center::MessageCenterTrayDelegate {
 public:
  explicit MessageCenterTrayBridge(
      message_center::MessageCenter* message_center);
  ~MessageCenterTrayBridge() override;

  // message_center::MessageCenterTrayDelegate:
  void OnMessageCenterTrayChanged() override;
  bool ShowPopups() override;
  void HidePopups() override;
  bool ShowMessageCenter(bool show_by_click) override;
  void HideMessageCenter() override;
  bool ShowNotifierSettings() override;
  message_center::MessageCenterTray* GetMessageCenterTray() override;

  message_center::MessageCenter* message_center() { return message_center_; }

 private:
  friend class MessageCenterTrayBridgeTest;

  // The global, singleton message center model object. Weak.
  message_center::MessageCenter* message_center_;

  // C++ controller for the notification tray UI.
  std::unique_ptr<message_center::MessageCenterTray> tray_;

  // Obj-C controller for the on-screen popup notifications.
  base::scoped_nsobject<MCPopupCollection> popup_collection_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterTrayBridge);
};

#endif  // CHROME_BROWSER_UI_COCOA_NOTIFICATIONS_MESSAGE_CENTER_TRAY_BRIDGE_H_
