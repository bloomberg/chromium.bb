// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_PLATFORM_IMPL_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_PLATFORM_IMPL_CHROMEOS_H_

#include <memory>

#include "base/time/time.h"

class RelaunchRecommendedTimer;
class RelaunchRequiredTimer;

class RelaunchNotificationControllerPlatformImpl {
 public:
  RelaunchNotificationControllerPlatformImpl();

  ~RelaunchNotificationControllerPlatformImpl();

  // Shows the relaunch recommended notification if it is not already open.
  void NotifyRelaunchRecommended(base::TimeTicks detection_time);

  // Shows the relaunch required notification if it is not already open.
  void NotifyRelaunchRequired(base::TimeTicks deadline);

  // Sets the notification title to the default one on Chrome OS.
  void CloseRelaunchNotification();

  // Sets the relaunch deadline to |deadline| and refreshes the notification's
  // title accordingly.
  void SetDeadline(base::TimeTicks deadline);

  // Returns true if relaunch required notification is shown.
  bool IsRequiredNotificationShown() const;

 private:
  // Callback triggered whenever the recommended notification's title has to
  // refresh.
  void RefreshRelaunchRecommendedTitle();

  // Callback triggered whenever the required notification's title has to
  // refresh.
  void RefreshRelaunchRequiredTitle();

  // Timer that takes care of the string refresh in the relaunch recommended
  // notification title.
  std::unique_ptr<RelaunchRecommendedTimer> relaunch_recommended_timer_;

  // Timer that takes care of the string refresh in the relaunch required
  // notification title.
  std::unique_ptr<RelaunchRequiredTimer> relaunch_required_timer_;

  DISALLOW_COPY_AND_ASSIGN(RelaunchNotificationControllerPlatformImpl);
};

#endif  // CHROME_BROWSER_UI_VIEWS_RELAUNCH_NOTIFICATION_RELAUNCH_NOTIFICATION_CONTROLLER_PLATFORM_IMPL_CHROMEOS_H_
