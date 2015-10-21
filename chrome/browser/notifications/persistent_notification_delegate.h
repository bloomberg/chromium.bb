// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_DELEGATE_H_
#define CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_DELEGATE_H_

#include <stdint.h>
#include <string>

#include "base/macros.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

// Delegate responsible for listening to the click event on persistent
// notifications, to forward them to the PlatformNotificationService so that
// JavaScript events can be fired on the associated Service Worker.
class PersistentNotificationDelegate : public NotificationDelegate {
 public:
  PersistentNotificationDelegate(content::BrowserContext* browser_context,
                                 int64_t persistent_notification_id,
                                 const GURL& origin,
                                 int notification_settings_index);

  // Persistent id of this notification in the notification database. To be
  // used when retrieving all information associated with it.
  int64_t persistent_notification_id() const {
    return persistent_notification_id_;
  }

  // NotificationDelegate implementation.
  void Display() override;
  void Close(bool by_user) override;
  void Click() override;
  void ButtonClick(int button_index) override;
  void SettingsClick() override;
  bool ShouldDisplaySettingsButton() override;

  std::string id() const override;

 protected:
  ~PersistentNotificationDelegate() override;

 private:
  content::BrowserContext* browser_context_;
  int64_t persistent_notification_id_;
  GURL origin_;
  std::string id_;
  int notification_settings_index_;

  DISALLOW_COPY_AND_ASSIGN(PersistentNotificationDelegate);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_DELEGATE_H_
