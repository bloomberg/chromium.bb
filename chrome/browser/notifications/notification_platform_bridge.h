// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "chrome/browser/notifications/notification_common.h"

class Notification;

// Provides the low-level interface that enables notifications to be displayed
// and interacted with on the user's screen, orthogonal of whether this
// functionality is provided by the browser or by the operating system.
// TODO(miguelg): Add support for click and close events.
class NotificationPlatformBridge {
 public:
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

  // Fills in |notifications| with a set of notification ids currently being
  // displayed for a given profile.
  // The return value expresses whether the underlying platform has the
  // capability to provide displayed notifications so the empty set
  // can be disambiguated.
  virtual bool GetDisplayed(const std::string& profile_id,
                            bool incognito,
                            std::set<std::string>* notification_ids) const = 0;

 protected:
  NotificationPlatformBridge() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridge);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_H_
