// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/notifications/message_center_tray_bridge.h"

#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/profiler/scoped_tracker.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/message_center/cocoa/popup_collection.h"
#import "ui/message_center/cocoa/status_item_view.h"
#import "ui/message_center/cocoa/tray_controller.h"
#import "ui/message_center/cocoa/tray_view_controller.h"
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
      status_item_update_pending_(false),
      weak_ptr_factory_(this) {
  show_status_item_.Init(
      prefs::kMessageCenterShowIcon,
      g_browser_process->local_state(),
      base::Bind(&MessageCenterTrayBridge::OnShowStatusItemChanged,
                 weak_ptr_factory_.GetWeakPtr()));
}

MessageCenterTrayBridge::~MessageCenterTrayBridge() {
  [status_item_view_ removeItem];
}

void MessageCenterTrayBridge::OnMessageCenterTrayChanged() {
  // Update the status item on the next run of the message loop so that if a
  // popup is displayed, the item doesn't flash the unread count.
  if (!status_item_update_pending_) {
    status_item_update_pending_ = true;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&MessageCenterTrayBridge::HandleMessageCenterTrayChanged,
                   weak_ptr_factory_.GetWeakPtr()));
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

bool MessageCenterTrayBridge::ShowMessageCenter() {
  if (tray_controller_)
    return false;

  OpenTrayWindow();
  return true;
}

void MessageCenterTrayBridge::HideMessageCenter() {
  [status_item_view_ setHighlight:NO];
  [tray_controller_ close];
  tray_controller_.autorelease();
  UpdateStatusItem();
}

bool MessageCenterTrayBridge::ShowNotifierSettings() {
  // This method needs to be implemented when the context menu of each
  // notification is ready and it contains 'settings' menu item.
  return false;
}

bool MessageCenterTrayBridge::IsContextMenuEnabled() const {
  // Cocoa message center doesn't support context menu yet.
  return false;
}

message_center::MessageCenterTray*
MessageCenterTrayBridge::GetMessageCenterTray() {
  return tray_.get();
}

void MessageCenterTrayBridge::UpdateStatusItem() {
  // We want a static message center icon while it's visible.
  if (!status_item_view_ || message_center()->IsMessageCenterVisible())
    return;

  size_t unread_count = message_center_->UnreadNotificationCount();
  bool quiet_mode = message_center_->IsQuietMode();
  [status_item_view_ setUnreadCount:unread_count withQuietMode:quiet_mode];

  if (unread_count > 0) {
    base::string16 unread_count_string = base::FormatNumber(unread_count);
    [status_item_view_ setToolTip:
        l10n_util::GetNSStringF(IDS_MESSAGE_CENTER_TOOLTIP_UNREAD,
            unread_count_string)];
  } else {
    [status_item_view_ setToolTip:
        l10n_util::GetNSString(IDS_MESSAGE_CENTER_TOOLTIP)];
  }
}

void MessageCenterTrayBridge::OpenTrayWindow() {
  DCHECK(!tray_controller_);
  tray_controller_.reset(
      [[MCTrayController alloc] initWithMessageCenterTray:tray_.get()]);
  [[tray_controller_ viewController] setTrayTitle:
      l10n_util::GetNSStringF(IDS_MESSAGE_CENTER_FOOTER_WITH_PRODUCT_TITLE,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME))];

  UpdateStatusItem();

  DCHECK(status_item_view_);
  [status_item_view_ setHighlight:YES];
  NSRect frame = [[status_item_view_ window] frame];
  [tray_controller_ showTrayAtRightOf:NSMakePoint(NSMinX(frame),
                                                  NSMinY(frame))
                             atLeftOf:NSMakePoint(NSMaxX(frame),
                                                  NSMinY(frame))];
}

bool MessageCenterTrayBridge::CanShowStatusItem() const {
  return g_browser_process->local_state()->GetBoolean(
      prefs::kMessageCenterShowIcon);
}

bool MessageCenterTrayBridge::NeedsStatusItem() const {
  return status_item_view_ || message_center_->NotificationCount() > 0;
}

void MessageCenterTrayBridge::ShowStatusItem() {
  if (status_item_view_)
    return;
  status_item_view_.reset([[MCStatusItemView alloc] init]);
  [status_item_view_ setCallback:^{
      tray_->ToggleMessageCenterBubble();
  }];
}

void MessageCenterTrayBridge::HideStatusItem() {
  [status_item_view_ removeItem];
  status_item_view_.reset();
}

void MessageCenterTrayBridge::HandleMessageCenterTrayChanged() {
  // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/465858
  // is fixed.
  tracked_objects::ScopedTracker tracking_profile1(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "465858 "
          "MessageCenterTrayBridge::HandleMessageCenterTrayChanged::Start"));
  status_item_update_pending_ = false;
  if (CanShowStatusItem() && NeedsStatusItem()) {
    // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/465858
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile2(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "465858 "
            "MessageCenterTrayBridge::HandleMessageCenterTrayChanged::"
            "ShowStatusItem"));
    ShowStatusItem();

    // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/465858
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile3(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "465858 "
            "MessageCenterTrayBridge::HandleMessageCenterTrayChanged::"
            "UpdateStatusItem"));
    UpdateStatusItem();
  } else {
    // TODO(erikchen): Remove ScopedTracker below once http://crbug.com/465858
    // is fixed.
    tracked_objects::ScopedTracker tracking_profile4(
        FROM_HERE_WITH_EXPLICIT_FUNCTION(
            "465858 "
            "MessageCenterTrayBridge::HandleMessageCenterTrayChanged::"
            "HideStatusItem"));
    HideStatusItem();
  }
}

void MessageCenterTrayBridge::OnShowStatusItemChanged() {
  if (CanShowStatusItem()) {
    ShowStatusItem();
    UpdateStatusItem();
  } else {
    HideStatusItem();
  }
}
