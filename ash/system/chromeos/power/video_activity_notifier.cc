// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/video_activity_notifier.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

namespace ash {
namespace {

// Minimum number of seconds between repeated notifications of the same state.
// This should be less than powerd's timeout for determining whether video is
// still active for the purposes of controlling the keyboard backlight.
const int kNotifyIntervalSec = 5;

}  // namespace

VideoActivityNotifier::VideoActivityNotifier(VideoDetector* detector)
    : detector_(detector),
      video_state_(detector->state()),
      screen_is_locked_(
          Shell::GetInstance()->session_state_delegate()->IsScreenLocked()) {
  detector_->AddObserver(this);
  WmShell::Get()->AddShellObserver(this);

  MaybeNotifyPowerManager();
  UpdateTimer();
}

VideoActivityNotifier::~VideoActivityNotifier() {
  WmShell::Get()->RemoveShellObserver(this);
  detector_->RemoveObserver(this);
}

void VideoActivityNotifier::OnVideoStateChanged(VideoDetector::State state) {
  if (video_state_ != state) {
    video_state_ = state;
    MaybeNotifyPowerManager();
    UpdateTimer();
  }
}

void VideoActivityNotifier::OnLockStateChanged(bool locked) {
  if (screen_is_locked_ != locked) {
    screen_is_locked_ = locked;
    MaybeNotifyPowerManager();
    UpdateTimer();
  }
}

bool VideoActivityNotifier::TriggerTimeoutForTest() {
  if (!notify_timer_.IsRunning())
    return false;

  MaybeNotifyPowerManager();
  return true;
}

void VideoActivityNotifier::UpdateTimer() {
  if (!should_notify_power_manager()) {
    notify_timer_.Stop();
  } else {
    notify_timer_.Start(FROM_HERE,
                        base::TimeDelta::FromSeconds(kNotifyIntervalSec), this,
                        &VideoActivityNotifier::MaybeNotifyPowerManager);
  }
}

void VideoActivityNotifier::MaybeNotifyPowerManager() {
  if (should_notify_power_manager()) {
    chromeos::DBusThreadManager::Get()
        ->GetPowerManagerClient()
        ->NotifyVideoActivity(video_state_ ==
                              VideoDetector::State::PLAYING_FULLSCREEN);
  }
}

}  // namespace ash
