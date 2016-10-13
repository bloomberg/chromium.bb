// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_HANDLER_H_

#include "base/macros.h"
#include "chrome/browser/notifications/notification_handler.h"

class NotificationDelegate;

// NotificationHandler implementation for persistent, service worker backed,
// notifications.
class PersistentNotificationHandler : public NotificationHandler {
 public:
  PersistentNotificationHandler();
  ~PersistentNotificationHandler() override;

  // NotificationHandler implementation.
  void OnClose(Profile* profile,
               const std::string& origin,
               const std::string& notification_id,
               bool by_user) override;
  void OnClick(Profile* profile,
               const std::string& origin,
               const std::string& notification_id,
               int action_index,
               const base::NullableString16& reply) override;
  void OpenSettings(Profile* profile) override;
  void RegisterNotification(const std::string& notification_id,
                            NotificationDelegate* delegate) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PersistentNotificationHandler);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_PERSISTENT_NOTIFICATION_HANDLER_H_
