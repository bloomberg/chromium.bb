// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/notifications/displayed_notifications_dispatch_callback.h"
#include "chrome/browser/notifications/notification_common.h"

namespace message_center {
class Notification;
}

// Provides the low-level interface that enables notifications to be displayed
// and interacted with on the user's screen, orthogonal of whether this
// functionality is provided by the browser or by the operating system.
// TODO(miguelg): Add support for click and close events.
class NotificationPlatformBridge {
 public:
  using NotificationBridgeReadyCallback =
      base::OnceCallback<void(bool /* success */)>;

  static NotificationPlatformBridge* Create();

  // Returns whether a native bridge can handle a notification of the given
  // type. Ideally, this would always return true, but for now some platforms
  // can't handle TRANSIENT notifications.
  static bool CanHandleType(NotificationCommon::Type notification_type);

  virtual ~NotificationPlatformBridge() {}

  // Shows a toast on screen using the data passed in |notification|.
  virtual void Display(
      NotificationCommon::Type notification_type,
      const std::string& profile_id,
      bool is_incognito,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata) = 0;

  // Closes a nofication with |notification_id| and |profile_id| if being
  // displayed.
  virtual void Close(const std::string& profile_id,
                     const std::string& notification_id) = 0;

  // Writes the ids of all currently displaying notifications and posts
  // |callback| with the result.
  virtual void GetDisplayed(
      const std::string& profile_id,
      bool incognito,
      const GetDisplayedNotificationsCallback& callback) const = 0;

  // Calls |callback| once |this| is initialized. The argument is
  // true if |this| is ready to be used and false if initialization
  // failed. |callback| may be called directly or from a posted task.
  virtual void SetReadyCallback(NotificationBridgeReadyCallback callback) = 0;

 protected:
  NotificationPlatformBridge() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridge);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_H_
