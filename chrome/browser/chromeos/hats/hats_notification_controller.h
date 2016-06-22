// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_HATS_HATS_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_HATS_HATS_NOTIFICATION_CONTROLLER_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"

class Profile;
class NetworkState;

namespace chromeos {

// Happiness tracking survey (HaTS) notification controller is responsible for
// managing the HaTS notification that is displayed to the user.
class HatsNotificationController : public NotificationDelegate,
                                   public NetworkPortalDetector::Observer {
 public:
  // Minimum amount of time before the notification is displayed again after a
  // user has interacted with it.
  static const base::TimeDelta kHatsThresholdTime;
  static const char kDelegateId[];
  static const char kNotificationId[];

  explicit HatsNotificationController(Profile* profile);

  // Returns true if the survey needs to be displayed for the given |profile|.
  static bool ShouldShowSurveyToProfile(Profile* profile);

 private:
  ~HatsNotificationController() override;

  // NotificationDelegate overrides:
  void ButtonClick(int button_index) override;
  void Close(bool by_user) override;
  std::string id() const override;

  // NetworkPortalDetector::Observer override:
  void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalState& state) override;

  Notification* CreateNotification();
  void UpdateLastInteractionTime();

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(HatsNotificationController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_HATS_HATS_NOTIFICATION_CONTROLLER_H_
