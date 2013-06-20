// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/notifications/message_center_tray_bridge.h"

#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/message_loop.h"
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
      tray_(new message_center::MessageCenterTray(this, message_center)),
      weak_ptr_factory_(this) {
}

MessageCenterTrayBridge::~MessageCenterTrayBridge() {
  [status_item_view_ removeItem];
}

void MessageCenterTrayBridge::OnMessageCenterTrayChanged() {
  // Update the status item on the next run of the message loop so that if a
  // popup is displayed, the item doesn't flash the unread count.
  base::MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&MessageCenterTrayBridge::UpdateStatusItem,
                 weak_ptr_factory_.GetWeakPtr()));

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
  if (tray_controller_)
    return false;

  // Post a task to open the window, because in
  // MessageCenterTray::ShowMessageCenterBubble, the unread count gets set to
  // 0 after it calls this delegate. In order show the window at the correct
  // position after the unread count is updated, opening the window must be
  // performed after the return of this method.
  base::MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&MessageCenterTrayBridge::OpenTrayWindow,
                 weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void MessageCenterTrayBridge::HideMessageCenter() {
  [status_item_view_ setHighlight:NO];
  [tray_controller_ close];
  tray_controller_.autorelease();
}

bool MessageCenterTrayBridge::ShowNotifierSettings() {
  // This method needs to be implemented when the context menu of each
  // notification is ready and it contains 'settings' menu item.
  return false;
}

void MessageCenterTrayBridge::UpdateStatusItem() {
  // Only show the status item if there are notifications.
  if (message_center_->NotificationCount() == 0) {
    [status_item_view_ removeItem];
    status_item_view_.reset();
    return;
  }

  if (!status_item_view_) {
    status_item_view_.reset([[MCStatusItemView alloc] init]);
    [status_item_view_ setCallback:^{ tray_->ToggleMessageCenterBubble(); }];
  }

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
}

void MessageCenterTrayBridge::OpenTrayWindow() {
  DCHECK(!tray_controller_);
  tray_controller_.reset(
      [[MCTrayController alloc] initWithMessageCenterTray:tray_.get()]);

  UpdateStatusItem();

  [status_item_view_ setHighlight:YES];
  NSRect frame = [[status_item_view_ window] frame];
  [tray_controller_ showTrayAtRightOf:NSMakePoint(NSMinX(frame),
                                                  NSMinY(frame))
                             atLeftOf:NSMakePoint(NSMaxX(frame),
                                                  NSMinY(frame))];
}
