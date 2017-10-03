// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_DISPLAY_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_DISPLAY_SERVICE_H_

#include "base/macros.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"

class Notification;
class Profile;

// Implementation of display service for notifications displayed by chrome
// instead of the native platform notification center.
class MessageCenterDisplayService : public NotificationDisplayService {
 public:
  explicit MessageCenterDisplayService(Profile* profile);
  ~MessageCenterDisplayService() override;

  // NotificationDisplayService implementation.
  void Display(NotificationCommon::Type notification_type,
               const std::string& notification_id,
               const Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;
  void Close(NotificationCommon::Type notification_type,
             const std::string& notification_id) override;
  void GetDisplayed(const DisplayedNotificationsCallback& callback) override;

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterDisplayService);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_DISPLAY_SERVICE_H_
