// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_DISPLAY_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_DISPLAY_SERVICE_H_

#include "base/macros.h"
#include "chrome/browser/notifications/notification_display_service.h"

class Notification;
class NotificationUIManager;
class Profile;

// Implementation of display service for notifications displayed by chrome
// instead of the native platform notification center.
class MessageCenterDisplayService : public NotificationDisplayService {
 public:
  MessageCenterDisplayService(Profile* profile,
                              NotificationUIManager* ui_manager);
  ~MessageCenterDisplayService() override;

  // NotificationDisplayService implementation.
  void Display(const std::string& notification_id,
               const Notification& notification) override;
  void Close(const std::string& notification_id) override;
  bool GetDisplayed(std::set<std::string>* notifications) const override;
  bool SupportsNotificationCenter() const override;

 private:
  Profile* profile_;
  NotificationUIManager* ui_manager_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterDisplayService);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_MESSAGE_CENTER_DISPLAY_SERVICE_H_
