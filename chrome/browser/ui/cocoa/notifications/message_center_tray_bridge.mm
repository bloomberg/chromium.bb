// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/notifications/message_center_tray_bridge.h"

#include "base/i18n/number_formatting.h"
#include "chrome/browser/browser_process.h"
#include "grit/chromium_strings.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/message_center/cocoa/popup_collection.h"
#import "ui/message_center/cocoa/status_item_view.h"
#import "ui/message_center/cocoa/tray_controller.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_tray.h"

namespace message_center {

MessageCenterTrayDelegate* CreateMessageCenterTray() {
  return new MessageCenterTrayBridge(g_browser_process->message_center());
}

}  // namespace message_center

MessageCenterTrayBridge::MessageCenterTrayBridge(
    message_center::MessageCenter* message_center)
    : message_center_(message_center),
      tray_(new message_center::MessageCenterTray(this, message_center)) {
  tray_controller_.reset(
      [[MCTrayController alloc] initWithMessageCenterTray:tray_.get()]);

  NSStatusBar* status_bar = [NSStatusBar systemStatusBar];
  status_item_view_.reset(
      [[MCStatusItemView alloc] initWithStatusItem:
          [status_bar statusItemWithLength:NSVariableStatusItemLength]]);
  [status_item_view_ setCallback:^{
      if ([[tray_controller_ window] isVisible])
        tray_->HideMessageCenterBubble();
      else
        tray_->ShowMessageCenterBubble();
  }];
}

MessageCenterTrayBridge::~MessageCenterTrayBridge() {
}

void MessageCenterTrayBridge::OnMessageCenterTrayChanged() {
  size_t unread_count = message_center_->UnreadNotificationCount();
  [status_item_view_ setUnreadCount:unread_count];

  string16 product_name = l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
  if (unread_count > 0) {
    string16 unread_count_string = base::FormatNumber(unread_count);
    [status_item_view_ setToolTip:
        l10n_util::GetNSStringF(IDS_MESSAGE_CENTER_TOOLTIP_UNREAD,
            product_name, unread_count_string)];
  } else {
    [status_item_view_ setToolTip:
        l10n_util::GetNSStringF(IDS_MESSAGE_CENTER_TOOLTIP, product_name)];
  }

  [tray_controller_ onMessageCenterTrayChanged];
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
  [status_item_view_ setHighlight:YES];
  NSRect frame = [[status_item_view_ window] frame];
  [tray_controller_ showTrayAt:NSMakePoint(NSMinX(frame), NSMinY(frame))];
  return true;
}

void MessageCenterTrayBridge::HideMessageCenter() {
  [status_item_view_ setHighlight:NO];
  [tray_controller_ close];
}
