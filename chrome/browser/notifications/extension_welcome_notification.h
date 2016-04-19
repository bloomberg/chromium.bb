// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_EXTENSION_WELCOME_NOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_EXTENSION_WELCOME_NOTIFICATION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_member.h"
#include "components/syncable_prefs/pref_service_syncable_observer.h"
#include "ui/message_center/notifier_settings.h"

namespace base {
typedef Callback<void(void)> Closure;
}

namespace message_center {
class MessageCenter;
}

namespace tracked_objects {
class Location;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class Notification;
class Profile;

// ExtensionWelcomeNotification is a keyed service which manages showing and
// hiding a welcome notification for built-in components that show
// notifications. The Welcome Notification presumes network connectivity since
// it relies on synced preferences to work. This is generally fine since the
// current consumers on the welcome notification also presume network
// connectivity.
//
// This class expects to be created and called from the UI thread.
class ExtensionWelcomeNotification
    : public KeyedService,
      public syncable_prefs::PrefServiceSyncableObserver {
 public:
  // Allows for overriding global calls.
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}
    virtual message_center::MessageCenter* GetMessageCenter() = 0;
    virtual base::Time GetCurrentTime() = 0;
    virtual void PostTask(
        const tracked_objects::Location& from_here,
        const base::Closure& task) = 0;
   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Requested time from showing the welcome notification to expiration.
  static const int kRequestedShowTimeDays;

  // The extension Id associated with the Google Now extension.
  static const char kChromeNowExtensionID[];

  ~ExtensionWelcomeNotification() override;

  // To workaround the lack of delegating constructors prior to C++11, we use
  // static Create methods.
  // Creates an ExtensionWelcomeNotification with the default delegate.
  static ExtensionWelcomeNotification* Create(Profile* const profile);

  // Creates an ExtensionWelcomeNotification with the specified delegate.
  static ExtensionWelcomeNotification* Create(Profile* const profile,
                                              Delegate* const delegate);

  // syncable_prefs::PrefServiceSyncableObserver
  void OnIsSyncingChanged() override;

  // Adds in the welcome notification if required for components built
  // into Chrome that show notifications like Chrome Now.
  void ShowWelcomeNotificationIfNecessary(const Notification& notification);

  // Handles Preference Registration for the Welcome Notification.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* prefs);

 private:
  enum PopUpRequest { POP_UP_HIDDEN = 0, POP_UP_SHOWN = 1, };

  ExtensionWelcomeNotification(
      Profile* const profile,
      ExtensionWelcomeNotification::Delegate* const delegate);

  // Gets the message center from the delegate.
  message_center::MessageCenter* GetMessageCenter() const;

  // Unconditionally shows the welcome notification.
  void ShowWelcomeNotification(const base::string16& display_source,
                               const PopUpRequest pop_up_request);

  // Hides the welcome notification.
  void HideWelcomeNotification();

  // Whether the notification has been dismissed.
  bool UserHasDismissedWelcomeNotification() const;

  // Called when the Welcome Notification Dismissed pref has been changed.
  void OnWelcomeNotificationDismissedChanged();

  // Starts the welcome notification expiration timer.
  void StartExpirationTimer();

  // Stops the welcome notification expiration timer.
  void StopExpirationTimer();

  // Expires the welcome notification by hiding it and marking it dismissed.
  void ExpireWelcomeNotification();

  // Gets the expiration timestamp or a null time is there is none.
  base::Time GetExpirationTimestamp() const;

  // Sets the expiration timestamp from now.
  void SetExpirationTimestampFromNow();

  // True if the welcome notification has expired, false otherwise.
  bool IsWelcomeNotificationExpired() const;

  // Prefs listener for welcome_notification_dismissed.
  BooleanPrefMember welcome_notification_dismissed_pref_;

  // Prefs listener for welcome_notification_dismissed_local.
  // Dismissal flag migrated from a synced pref to a local one.
  BooleanPrefMember welcome_notification_dismissed_local_pref_;

  // The notifier for the extension that we're listening for.
  message_center::NotifierId notifier_id_;

  // The profile which owns this object.
  Profile* profile_;

  // Notification ID of the Welcome Notification.
  std::string welcome_notification_id_;

  // If the preferences are still syncing, store the last notification here
  // so we can replay ShowWelcomeNotificationIfNecessary once the sync finishes.
  // Simplifying Assumption: The delayed notification has passed the
  // extension ID check. This means we do not need to store all of the
  // notifications that may also show a welcome notification.
  std::unique_ptr<Notification> delayed_notification_;

  // If the welcome notification is shown, this timer tracks when to hide the
  // welcome notification.
  std::unique_ptr<base::OneShotTimer> expiration_timer_;

  // Delegate for Chrome global calls like base::Time::GetTime() for
  // testability.
  std::unique_ptr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWelcomeNotification);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_EXTENSION_WELCOME_NOTIFICATION_H_
