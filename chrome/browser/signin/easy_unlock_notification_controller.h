// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_NOTIFICATION_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/proximity_auth/notification_controller.h"

class Profile;

// Responsible for displaying all notifications for EasyUnlock.
class EasyUnlockNotificationController
    : public proximity_auth::NotificationController {
 public:
  // Creates an instance of the EasyUnlockNotificationController.
  static std::unique_ptr<EasyUnlockNotificationController> Create(
      Profile* profile);

  EasyUnlockNotificationController() {}
  ~EasyUnlockNotificationController() override {}

  // proximity_auth::NotificationController:
  void ShowChromebookAddedNotification() override = 0;
  void ShowPairingChangeNotification() override = 0;
  void ShowPairingChangeAppliedNotification(
      const std::string& phone_name) override = 0;
  void ShowPromotionNotification() override = 0;
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_NOTIFICATION_CONTROLLER_H_
