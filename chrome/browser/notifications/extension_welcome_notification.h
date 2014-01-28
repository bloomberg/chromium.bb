// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_EXTENSION_WELCOME_NOTIFICATION_H_
#define CHROME_BROWSER_NOTIFICATIONS_EXTENSION_WELCOME_NOTIFICATION_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/timer/timer.h"
#include "chrome/browser/prefs/pref_service_syncable_observer.h"
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

// ExtensionWelcomeNotification is a part of DesktopNotificationService and
// manages showing and hiding a welcome notification for built-in components
// that show notifications. The Welcome Notification presumes network
// connectivity since it relies on synced preferences to work. This is generally
// fine since the current consumers on the welcome notification also presume
// network connectivity.
class ExtensionWelcomeNotification : public PrefServiceSyncableObserver {
 public:
  // Requested time from showing the welcome notification to expiration.
  static const unsigned int kRequestedShowTimeDays;

  // Allows for overriding global calls.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual message_center::MessageCenter* GetMessageCenter() = 0;
    virtual base::Time GetCurrentTime() = 0;
    virtual void PostTask(
        const tracked_objects::Location& from_here,
        const base::Closure& task) = 0;
  };

  // To workaround the lack of delegating constructors prior to C++11, we use
  // static Create methods.
  // Creates an ExtensionWelcomeNotification with the default delegate.
  static scoped_ptr<ExtensionWelcomeNotification> Create(
      const std::string& extension_id,
      Profile* profile);

  // Creates an ExtensionWelcomeNotification with the specified delegate.
  static scoped_ptr<ExtensionWelcomeNotification> Create(
      const std::string& extension_id,
      Profile* profile,
      Delegate* delegate);

  virtual ~ExtensionWelcomeNotification();

  // PrefServiceSyncableObserver
  virtual void OnIsSyncingChanged() OVERRIDE;

  // Adds in a the welcome notification if required for components built
  // into Chrome that show notifications like Chrome Now.
  void ShowWelcomeNotificationIfNecessary(const Notification& notification);

  // Handles Preference Registeration for the Welcome Notification.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* prefs);

 private:
  ExtensionWelcomeNotification(
      const std::string& extension_id,
      Profile* profile,
      ExtensionWelcomeNotification::Delegate* delegate);

  enum PopUpRequest { POP_UP_HIDDEN = 0, POP_UP_SHOWN = 1, };

  // Gets the message center from the delegate.
  message_center::MessageCenter* GetMessageCenter();

  // Unconditionally shows the welcome notification.
  void ShowWelcomeNotification(const base::string16& display_source,
                               PopUpRequest pop_up_request);

  // Hides the welcome notification.
  void HideWelcomeNotification();

  // Called when the Welcome Notification Dismissed pref has been changed.
  void OnWelcomeNotificationDismissedChanged();

  // Starts the welcome notification expiration timer.
  void StartExpirationTimer();

  // Stops the welcome notification expiration timer.
  void StopExpirationTimer();

  // Expires the welcome notification by hiding it and marking it dismissed.
  void ExpireWelcomeNotification();

  // Gets the expiration timestamp or a null time is there is none.
  base::Time GetExpirationTimestamp();

  // Sets the expiration timestamp from now.
  void SetExpirationTimestampFromNow();

  // True if the welcome notification has expired, false otherwise.
  bool IsWelcomeNotificationExpired();

  // Prefs listener for welcome_notification_dismissed.
  BooleanPrefMember welcome_notification_dismissed_pref_;

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
  scoped_ptr<Notification> delayed_notification_;

  // If the welcome notification is shown, this timer tracks when to hide the
  // welcome notification.
  scoped_ptr<base::OneShotTimer<ExtensionWelcomeNotification> >
      expiration_timer_;

  // Delegate for global calls.
  scoped_ptr<Delegate> delegate_;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_EXTENSION_WELCOME_NOTIFICATION_H_
