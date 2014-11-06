// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_PROFILE_NOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_PROFILE_NOTIFICATION_H_

#include <string>

#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"

// This class keeps a Notification objects and its corresponding Profile, so
// that when the Notification UI manager needs to return or cancel all
// notifications for a given Profile we have the ability to do this.
class ProfileNotification {
 public:
  // Returns a string that uniquely identifies a profile + delegate_id pair.
  // The profile_id is used as an identifier to identify a profile instance; it
  // cannot be NULL. The ID becomes invalid when a profile is destroyed.
  static std::string GetProfileNotificationId(const std::string& delegate_id,
                                              ProfileID profile_id);

  ProfileNotification(Profile* profile, const Notification& notification);
  ~ProfileNotification();

  Profile* profile() const { return profile_; }
  const Notification& notification() const { return notification_; }

 private:
  // Weak, guaranteed not to be used after profile removal by parent class.
  Profile* profile_;

  Notification notification_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_PROFILE_NOTIFICATION_H_
