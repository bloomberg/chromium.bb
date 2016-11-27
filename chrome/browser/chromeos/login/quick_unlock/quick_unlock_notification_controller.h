// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_NOTIFICATION_CONTROLLER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;
class Notification;

namespace chromeos {

// Quick Unlock feature notification controller is responsible for managing the
// new feature notification displayed to the user.
class QuickUnlockNotificationController : public NotificationDelegate,
                                          public content::NotificationObserver {
 public:
  explicit QuickUnlockNotificationController(Profile* profile);

  // Returns true if the notification needs to be displayed for the given
  // |profile|.
  static bool ShouldShow(Profile* profile);

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  ~QuickUnlockNotificationController() override;

  // NotificationDelegate overrides:
  void Close(bool by_user) override;
  void Click() override;
  std::string id() const override;

  Notification* CreateNotification();
  void UnregisterObserver();

  Profile* profile_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(QuickUnlockNotificationController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_NOTIFICATION_CONTROLLER_H_
