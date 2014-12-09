// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_PERSISTENT_NOTIFICATION_STATUS_H_
#define CONTENT_PUBLIC_COMMON_PERSISTENT_NOTIFICATION_STATUS_H_

namespace content {

// Delivery status for persistent notification clicks to a Service Worker.
enum PersistentNotificationStatus {
  // The notificationclick event has been delivered successfully.
  PERSISTENT_NOTIFICATION_STATUS_SUCCESS = 0,

  // The event could not be delivered because the Service Worker is unavailable.
  PERSISTENT_NOTIFICATION_STATUS_NO_SERVICE_WORKER,

  // The event could not be delivered because of a Service Worker error.
  PERSISTENT_NOTIFICATION_STATUS_SERVICE_WORKER_ERROR,

  // The event has been delivered, but the developer extended the event with a
  // promise that has been rejected.
  PERSISTENT_NOTIFICATION_STATUS_EVENT_WAITUNTIL_REJECTED
};

}  // content

#endif  // CONTENT_PUBLIC_COMMON_PERSISTENT_NOTIFICATION_STATUS_H_
