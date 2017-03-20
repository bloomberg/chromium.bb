// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/notifications/notification_common.h"
#include "components/keyed_service/core/keyed_service.h"

class Notification;
class Profile;

// Profile-bound service that enables notifications to be displayed and
// interacted with on the user's screen, orthogonal of whether this
// functionality is provided by the browser or by the operating system. An
// instance can be retrieved through the NotificationDisplayServiceFactory.
//
// TODO(peter): Add a NotificationHandler mechanism for registering listeners.
class NotificationDisplayService : public KeyedService {
 public:
  using DisplayedNotificationsCallback =
      base::Callback<void(std::unique_ptr<std::set<std::string>>,
                          bool /* supports_synchronization */)>;
  NotificationDisplayService() {}
  ~NotificationDisplayService() override {}

  // Displays the |notification| identified by |notification_id|.
  virtual void Display(NotificationCommon::Type notification_type,
                       const std::string& notification_id,
                       const Notification& notification) = 0;

  // Closes the notification identified by |notification_id|.
  virtual void Close(NotificationCommon::Type notification_type,
                     const std::string& notification_id) = 0;

  // Writes the ids of all currently displaying notifications and
  // invokes |callback| with the result once known.
  virtual void GetDisplayed(
      const DisplayedNotificationsCallback& callback) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationDisplayService);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_H_
