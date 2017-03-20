// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_H_

#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/notifications/notification_common.h"

class Notification;

// Provides the low-level interface that enables notifications to be displayed
// and interacted with on the user's screen, orthogonal of whether this
// functionality is provided by the browser or by the operating system.
// TODO(miguelg): Add support for click and close events.
class NotificationPlatformBridge {
 public:
  using DisplayedNotificationsCallback =
      base::Callback<void(std::unique_ptr<std::set<std::string>>,
                          bool /* supports_synchronization */)>;

  static NotificationPlatformBridge* Create();

  virtual ~NotificationPlatformBridge() {}

  // Shows a toast on screen using the data passed in |notification|.
  virtual void Display(NotificationCommon::Type notification_type,
                       const std::string& notification_id,
                       const std::string& profile_id,
                       bool is_incognito,
                       const Notification& notification) = 0;

  // Closes a nofication with |notification_id| and |profile_id| if being
  // displayed.
  virtual void Close(const std::string& profile_id,
                     const std::string& notification_id) = 0;

  // Writes the ids of all currently displaying notifications and posts
  // |callback| with the result.
  virtual void GetDisplayed(
      const std::string& profile_id,
      bool incognito,
      const DisplayedNotificationsCallback& callback) const = 0;

 protected:
  NotificationPlatformBridge() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridge);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_H_
