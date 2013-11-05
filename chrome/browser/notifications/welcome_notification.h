// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_WELCOME_NOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_WELCOME_NOTIFICATION_H_

#include <string>

#include "base/prefs/pref_member.h"
#include "ui/message_center/notifier_settings.h"

namespace message_center {
class MessageCenter;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class Notification;
class Profile;

// WelcomeNotification is a part of DesktopNotificationService and manages
// showing and hiding a welcome notification for built-in components that
// show notifications.
class WelcomeNotification {
 public:
  WelcomeNotification(
      Profile* profile,
      message_center::MessageCenter* message_center);
  ~WelcomeNotification();

  // Adds in a the welcome notification if required for components built
  // into Chrome that show notifications like Chrome Now.
  void ShowWelcomeNotificationIfNecessary(
      const Notification& notification);

  // Handles Preference Registeration for the Welcome Notification.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* prefs);

 private:

  enum PopUpRequest {
    POP_UP_HIDDEN = 0,
    POP_UP_SHOWN = 1,
  };

  // Unconditionally shows the welcome notification.
  void ShowWelcomeNotification(
      const message_center::NotifierId notifier_id,
      const string16& display_source,
      PopUpRequest pop_up_request);

  // Hides the welcome notification.
  void HideWelcomeNotification();

  // Called when the Welcome Notification Dismissed pref has been changed.
  void OnWelcomeNotificationDismissedChanged();

  // Prefs listener for welcome_notification_dismissed.
  BooleanPrefMember welcome_notification_dismissed_pref_;

  // The profile which owns this object.
  Profile* profile_;

  // Notification ID of the Welcome Notification.
  std::string welcome_notification_id_;

  message_center::MessageCenter* message_center_;  // Weak reference.
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_WELCOME_NOTIFICATION_H_
