// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NOTIFICATION_EVENT_DISPATCHER_H_
#define CONTENT_PUBLIC_BROWSER_NOTIFICATION_EVENT_DISPATCHER_H_

#include <string>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/common/persistent_notification_status.h"

class GURL;

namespace content {

class BrowserContext;
struct PlatformNotificationData;

// This is the dispatcher to be used for firing events related to persistent
// notifications on a Service Worker. This class is a singleton, the instance of
// which can be retrieved using the static GetInstance() method. All methods
// must be called on the UI thread.
class CONTENT_EXPORT NotificationEventDispatcher {
 public:
  static NotificationEventDispatcher* GetInstance();

  using NotificationClickDispatchCompleteCallback =
      base::Callback<void(PersistentNotificationStatus)>;

  // Dispatches the "notificationclick" event on the Service Worker associated
  // with |origin| and |service_worker_registration_id|. The |callback| will
  // be invoked when it's known whether the event successfully executed.
  virtual void DispatchNotificationClickEvent(
      BrowserContext* browser_context,
      const GURL& origin,
      int64 service_worker_registration_id,
      const std::string& notification_id,
      const PlatformNotificationData& notification_data,
      const NotificationClickDispatchCompleteCallback&
          dispatch_complete_callback) = 0;

 protected:
  virtual ~NotificationEventDispatcher() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NOTIFICATION_EVENT_DISPATCHER_H_
