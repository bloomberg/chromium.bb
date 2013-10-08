// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/power/video_activity_notifier.h"

#include "ash/shell.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

namespace ash {
namespace internal {

namespace {

// Minimum number of seconds between notifications.
const int kNotifyIntervalSec = 5;

}  // namespace

VideoActivityNotifier::VideoActivityNotifier(VideoDetector* detector)
    : detector_(detector) {
  detector_->AddObserver(this);
}

VideoActivityNotifier::~VideoActivityNotifier() {
  detector_->RemoveObserver(this);
}

void VideoActivityNotifier::OnVideoDetected(bool is_fullscreen) {
  base::TimeTicks now = base::TimeTicks::Now();
  if (last_notify_time_.is_null() ||
      (now - last_notify_time_).InSeconds() >= kNotifyIntervalSec) {
    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
        NotifyVideoActivity(is_fullscreen);
    last_notify_time_ = now;
  }
}

}  // namespace internal
}  // namespace ash
