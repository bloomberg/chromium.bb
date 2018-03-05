// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_NOTIFICATION_CONTROLLER_CHROMEOS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_NOTIFICATION_CONTROLLER_CHROMEOS_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_notification_controller.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class Profile;

namespace chromeos {

// Implementation of EasyUnlockNotificationController for ChromeOS.
class EasyUnlockNotificationControllerChromeOS
    : public EasyUnlockNotificationController {
 public:
  explicit EasyUnlockNotificationControllerChromeOS(Profile* profile);
  ~EasyUnlockNotificationControllerChromeOS() override;

  // EasyUnlockNotificationController:
  void ShowChromebookAddedNotification() override;
  void ShowPairingChangeNotification() override;
  void ShowPairingChangeAppliedNotification(
      const std::string& phone_name) override;
  void ShowPromotionNotification() override;

 protected:
  // Exposed for testing
  virtual void LaunchEasyUnlockSettings();
  virtual void LockScreen();

 private:
  // NotificationDelegate implementation for handling click events.
  class NotificationDelegate : public message_center::NotificationDelegate {
   public:
    NotificationDelegate(
        const std::string& notification_id,
        const base::WeakPtr<EasyUnlockNotificationControllerChromeOS>&
            notification_controller);

    // message_center::NotificationDelegate:
    void Click() override;
    void ButtonClick(int button_index) override;

   private:
    ~NotificationDelegate() override;

    std::string notification_id_;
    base::WeakPtr<EasyUnlockNotificationControllerChromeOS>
        notification_controller_;

    DISALLOW_COPY_AND_ASSIGN(NotificationDelegate);
  };

  // Displays the notification to the user.
  void ShowNotification(
      std::unique_ptr<message_center::Notification> notification);

  Profile* profile_;

  base::WeakPtrFactory<EasyUnlockNotificationControllerChromeOS>
      weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockNotificationControllerChromeOS);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_EASY_UNLOCK_NOTIFICATION_CONTROLLER_CHROMEOS_H_
