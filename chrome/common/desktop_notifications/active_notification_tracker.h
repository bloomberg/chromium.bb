// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_DESKTOP_NOTIFICATIONS_ACTIVE_NOTIFICATION_TRACKER_H_
#define CHROME_COMMON_DESKTOP_NOTIFICATIONS_ACTIVE_NOTIFICATION_TRACKER_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/hash_tables.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotification.h"

namespace WebKit {
class WebNotificationPermissionCallback;
}

// This class manages the set of active Notification objects in either
// a render or worker process.  This class should be accessed only on
// the main thread.
class ActiveNotificationTracker {
 public:
  ActiveNotificationTracker();
  ~ActiveNotificationTracker();

  // Methods for tracking active notification objects.
  int RegisterNotification(const WebKit::WebNotification& notification);
  void UnregisterNotification(int id);
  bool GetId(const WebKit::WebNotification& notification, int& id);
  bool GetNotification(int id, WebKit::WebNotification* notification);

  // Methods for tracking active permission requests.
  int RegisterPermissionRequest(
      WebKit::WebNotificationPermissionCallback* callback);
  void OnPermissionRequestComplete(int id);
  WebKit::WebNotificationPermissionCallback* GetCallback(int id);

  // Clears out all active notifications.  Useful on page navigation.
  void Clear();

  // Detaches all active notifications from their presenter.  Necessary
  // when the Presenter is destroyed.
  void DetachAll();

 private:
  typedef std::map<WebKit::WebNotification, int> ReverseTable;

  // Tracking maps for active notifications and permission requests.
  IDMap<WebKit::WebNotification> notification_table_;
  ReverseTable reverse_notification_table_;
  IDMap<WebKit::WebNotificationPermissionCallback> callback_table_;

  DISALLOW_COPY_AND_ASSIGN(ActiveNotificationTracker);
};

#endif  // CHROME_COMMON_DESKTOP_NOTIFICATIONS_ACTIVE_NOTIFICATION_TRACKER_H_
