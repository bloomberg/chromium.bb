// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_list.h"

#include "ash/system/web_notification/web_notification_tray.h"

namespace ash {

namespace message_center {

WebNotificationList::WebNotificationList()
    : message_center_visible_(false),
      unread_count_(0) {
}

WebNotificationList::~WebNotificationList() {
}

void WebNotificationList::SetMessageCenterVisible(bool visible) {
  if (message_center_visible_ == visible)
    return;
  message_center_visible_ = visible;
  if (!visible) {
    // When the list is hidden, clear the unread count, and mark all
    // notifications as read and shown.
    unread_count_ = 0;
    for (Notifications::iterator iter = notifications_.begin();
         iter != notifications_.end(); ++iter) {
      iter->is_read = true;
      iter->shown_as_popup = true;
    }
  }
}

void WebNotificationList::AddNotification(const std::string& id,
                                          const string16& title,
                                          const string16& message,
                                          const string16& display_source,
                                          const std::string& extension_id) {
  WebNotification notification;
  notification.id = id;
  notification.title = title;
  notification.message = message;
  notification.display_source = display_source;
  notification.extension_id = extension_id;
  PushNotification(notification);
}

void WebNotificationList::UpdateNotificationMessage(const std::string& old_id,
                                                    const std::string& new_id,
                                                    const string16& title,
                                                    const string16& message) {
  Notifications::iterator iter = GetNotification(old_id);
  if (iter == notifications_.end())
    return;
  // Copy and update notification, then move it to the front of the list.
  WebNotification notification(*iter);
  notification.id = new_id;
  notification.title = title;
  notification.message = message;
  EraseNotification(iter);
  PushNotification(notification);
}

bool WebNotificationList::RemoveNotification(const std::string& id) {
  Notifications::iterator iter = GetNotification(id);
  if (iter == notifications_.end())
    return false;
  EraseNotification(iter);
  return true;
}

void WebNotificationList::RemoveAllNotifications() {
  notifications_.clear();
}

void WebNotificationList::SendRemoveNotificationsBySource(
    WebNotificationTray* tray,
    const std::string& id) {
  Notifications::iterator source_iter = GetNotification(id);
  if (source_iter == notifications_.end())
    return;
  string16 display_source = source_iter->display_source;
  for (Notifications::iterator loopiter = notifications_.begin();
       loopiter != notifications_.end(); ) {
    Notifications::iterator curiter = loopiter++;
    if (curiter->display_source == display_source)
      tray->SendRemoveNotification(curiter->id);
  }
}

void WebNotificationList::SendRemoveNotificationsByExtension(
    WebNotificationTray* tray,
    const std::string& id) {
  Notifications::iterator source_iter = GetNotification(id);
  if (source_iter == notifications_.end())
    return;
  std::string extension_id = source_iter->extension_id;
  for (Notifications::iterator loopiter = notifications_.begin();
       loopiter != notifications_.end(); ) {
    Notifications::iterator curiter = loopiter++;
    if (curiter->extension_id == extension_id)
      tray->SendRemoveNotification(curiter->id);
  }
}

bool WebNotificationList::SetNotificationImage(const std::string& id,
                                               const gfx::ImageSkia& image) {
  Notifications::iterator iter = GetNotification(id);
  if (iter == notifications_.end())
    return false;
  iter->image = image;
  return true;
}

bool WebNotificationList::HasNotification(const std::string& id) {
  return GetNotification(id) != notifications_.end();
}

bool WebNotificationList::HasPopupNotifications() {
  return !notifications_.empty() && !notifications_.front().shown_as_popup;
}

void WebNotificationList::GetPopupNotifications(
    WebNotificationList::Notifications* notifications) {
  Notifications::iterator first, last;
  GetPopupIterators(first, last);
  notifications->clear();
  for (Notifications::iterator iter = first; iter != last; ++iter)
    notifications->push_back(*iter);
}

void WebNotificationList::MarkPopupsAsShown() {
  Notifications::iterator first, last;
  GetPopupIterators(first, last);
  for (Notifications::iterator iter = first; iter != last; ++iter)
    iter->shown_as_popup = true;
}

WebNotificationList::Notifications::iterator
WebNotificationList::GetNotification(
    const std::string& id) {
  for (Notifications::iterator iter = notifications_.begin();
       iter != notifications_.end(); ++iter) {
    if (iter->id == id)
      return iter;
  }
  return notifications_.end();
}

void WebNotificationList::EraseNotification(
    WebNotificationList::Notifications::iterator iter) {
  if (!message_center_visible_ && !iter->is_read)
    --unread_count_;
  notifications_.erase(iter);
}

void WebNotificationList::PushNotification(WebNotification& notification) {
  // Ensure that notification.id is unique by erasing any existing
  // notification with the same id (shouldn't normally happen).
  Notifications::iterator iter = GetNotification(notification.id);
  if (iter != notifications_.end())
    EraseNotification(iter);
  // Add the notification to the front (top) of the list and mark it
  // unread and unshown.
  if (!message_center_visible_) {
    ++unread_count_;
    notification.is_read = false;
    notification.shown_as_popup = false;
  }
  notifications_.push_front(notification);
}

void WebNotificationList::GetPopupIterators(Notifications::iterator& first,
                                            Notifications::iterator& last) {
  size_t popup_count = 0;
  first = notifications_.begin();
  last = first;
  while (last != notifications_.end()) {
    if (last->shown_as_popup)
      break;
    ++last;
    if (popup_count < WebNotificationTray::kMaxVisiblePopupNotifications)
      ++popup_count;
    else
      ++first;
  }
}

}  // namespace message_center

}  // namespace ash
