// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/desktop_notifications/active_notification_tracker.h"

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNotification.h"
#include "third_party/WebKit/WebKit/chromium/public/WebNotificationPermissionCallback.h"

using WebKit::WebNotification;
using WebKit::WebNotificationPermissionCallback;

bool ActiveNotificationTracker::GetId(
    const WebNotification& notification, int& id) {
  ReverseTable::iterator iter = reverse_notification_table_.find(notification);
  if (iter == reverse_notification_table_.end())
    return false;
  id = iter->second;
  return true;
}

bool ActiveNotificationTracker::GetNotification(
    int id, WebNotification* notification) {
  WebNotification* lookup = notification_table_.Lookup(id);
  if (!lookup)
    return false;

  *notification = *lookup;
  return true;
}

int ActiveNotificationTracker::RegisterNotification(
    const WebKit::WebNotification& proxy) {
  WebNotification* notification = new WebNotification(proxy);
  int id = notification_table_.Add(notification);
  reverse_notification_table_[proxy] = id;
  return id;
}

void ActiveNotificationTracker::UnregisterNotification(int id) {
  // We want to free the notification after removing it from the table.
  scoped_ptr<WebNotification> notification(notification_table_.Lookup(id));
  notification_table_.Remove(id);
  DCHECK(notification.get());
  if (notification.get())
    reverse_notification_table_.erase(*notification);
}

void ActiveNotificationTracker::Clear() {
  while (!reverse_notification_table_.empty()) {
    ReverseTable::iterator iter = reverse_notification_table_.begin();
    UnregisterNotification((*iter).second);
  }
}

void ActiveNotificationTracker::DetachAll() {
  ReverseTable::iterator iter;
  for (iter = reverse_notification_table_.begin();
       iter != reverse_notification_table_.end();
       ++iter) {
    WebNotification notification(iter->first);
    notification.detachPresenter();
  }
}

WebNotificationPermissionCallback* ActiveNotificationTracker::GetCallback(
    int id) {
  return callback_table_.Lookup(id);
}

int ActiveNotificationTracker::RegisterPermissionRequest(
    WebNotificationPermissionCallback* callback) {
  return callback_table_.Add(callback);
}

void ActiveNotificationTracker::OnPermissionRequestComplete(int id) {
  callback_table_.Remove(id);
}
