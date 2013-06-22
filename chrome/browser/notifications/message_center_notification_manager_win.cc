// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/message_center_notification_manager.h"

#include "chrome/common/pref_names.h"
#include "ui/message_center/message_center_tray.h"

void MessageCenterNotificationManager::DisplayFirstRunBalloon() {
  // Store for posterity the fact that we've shown the first-run balloon.
  DCHECK(tray_.get());
  first_run_pref_.SetValue(true);
  tray_->DisplayFirstRunBalloon();
}

void MessageCenterNotificationManager::SetFirstRunTimeoutForTest(
    base::TimeDelta timeout) {
  first_run_idle_timeout_ = timeout;
}

bool MessageCenterNotificationManager::FirstRunTimerIsActive() const {
  return first_run_balloon_timer_.IsRunning();
}

void MessageCenterNotificationManager::CheckFirstRunTimer() {
  // If there is no tray_, we can't display a balloon here anyway.
  // Also, we only want to display the first run balloon once, so the pref will
  // store the flag on disk based on whether we ever showed the balloon.
  DCHECK(tray_.get());
  if (first_run_pref_.GetValue())
    return;

  // If there are popups, the message center is visible, or there are no more
  // notifications, don't continue the timer since it will be annoying or
  // useless.
  if (message_center_->HasPopupNotifications() ||
      message_center_->IsMessageCenterVisible() ||
      0 == message_center_->NotificationCount()) {
    first_run_balloon_timer_.Stop();
    return;
  }

  // No need to restart the timer if it's already going.
  if (first_run_balloon_timer_.IsRunning())
    return;

  first_run_balloon_timer_.Start(
      FROM_HERE,
      first_run_idle_timeout_,
      base::Bind(&MessageCenterNotificationManager::DisplayFirstRunBalloon,
                 weak_factory_.GetWeakPtr()));
}
