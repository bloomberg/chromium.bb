// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_NOTIFICATION_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/macros.h"

class Profile;

// Responsible for displaying all notifications for EasyUnlock.
class EasyUnlockNotificationController {
 public:
  // Creates an instance of the EasyUnlockNotificationController.
  static std::unique_ptr<EasyUnlockNotificationController> Create(
      Profile* profile);

  EasyUnlockNotificationController() {}
  virtual ~EasyUnlockNotificationController() {}

  // Shows the notification when EasyUnlock is synced to a new Chromebook.
  virtual void ShowChromebookAddedNotification() = 0;

  // Shows the notification when EasyUnlock is already enabled on a Chromebook,
  // but a different phone is synced as the unlock key.
  virtual void ShowPairingChangeNotification() = 0;

  // Shows the notification after password reauth confirming that the new phone
  // should be used for EasyUnlock from now on.
  virtual void ShowPairingChangeAppliedNotification(
      const std::string& phone_name) = 0;

  // Shows the notification to promote EasyUnlock to the user.
  virtual void ShowPromotionNotification() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(EasyUnlockNotificationController);
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_NOTIFICATION_CONTROLLER_H_
