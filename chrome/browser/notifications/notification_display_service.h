// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_H_

#include <set>
#include <string>
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

class Notification;
class NotificationPlatformBridge;
class Profile;

// Profile-bound service that enables notifications to be displayed and
// interacted with on the user's screen, orthogonal of whether this
// functionality is provided by the browser or by the operating system. An
// instance can be retrieved through the NotificationDisplayServiceFactory.
//
// TODO(peter): Add a NotificationHandler mechanism for registering listeners.
// TODO(miguelg): Remove the SupportsNotificationCenter method.
class NotificationDisplayService : public KeyedService {
 public:
  NotificationDisplayService() {}
  ~NotificationDisplayService() override {}

  // Displays the |notification| identified by |notification_id|.
  virtual void Display(const std::string& notification_id,
                       const Notification& notification) = 0;

  // Closes the notification identified by |notification_id|.
  virtual void Close(const std::string& notification_id) = 0;

  // Returns whether the implementation can retrieve a list of currently visible
  // notifications and stores them in |*notification_ids| when possible.
  virtual bool GetDisplayed(std::set<std::string>* notifications) const = 0;

  // Temporary method while we finish the refactor. Returns whether there is
  // a native notification center backing up notifications.
  virtual bool SupportsNotificationCenter() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationDisplayService);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_H_
