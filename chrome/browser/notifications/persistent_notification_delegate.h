// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_DELEGATE_H_
#define CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_DELEGATE_H_

#include <string>

#include "chrome/browser/notifications/notification_delegate.h"
#include "content/public/common/platform_notification_data.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

// Delegate responsible for listening to the click event on persistent
// notifications, to forward them to the PlatformNotificationService so that
// JavaScript events can be fired on the associated Service Worker.
class PersistentNotificationDelegate : public NotificationDelegate {
 public:
  PersistentNotificationDelegate(
      content::BrowserContext* browser_context,
      int64 service_worker_registration_id,
      const GURL& origin,
      const content::PlatformNotificationData& notification_data);

  int64 service_worker_registration_id() const {
    return service_worker_registration_id_;
  }

  const content::PlatformNotificationData& notification_data() const {
    return notification_data_;
  }

  // NotificationDelegate implementation.
  void Display() override;
  void Close(bool by_user) override;
  void Click() override;
  std::string id() const override;

 protected:
  ~PersistentNotificationDelegate() override;

 private:
  content::BrowserContext* browser_context_;
  int64 service_worker_registration_id_;
  GURL origin_;
  content::PlatformNotificationData notification_data_;

  std::string id_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_DELEGATE_H_
