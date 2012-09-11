// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/video_activity_notifier.h"

#include "ash/shell.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

namespace {

// Minimum number of seconds between notifications.
const int kNotifyIntervalSec = 5;

}  // namespace

namespace chromeos {

VideoActivityNotifier::VideoActivityNotifier() {
  ash::Shell::GetInstance()->video_detector()->AddObserver(this);
}

VideoActivityNotifier::~VideoActivityNotifier() {
  ash::Shell::GetInstance()->video_detector()->RemoveObserver(this);
}

void VideoActivityNotifier::OnVideoDetected(bool is_fullscreen) {
  base::TimeTicks now = base::TimeTicks::Now();
  if (last_notify_time_.is_null() ||
      (now - last_notify_time_).InSeconds() >= kNotifyIntervalSec) {
    DBusThreadManager::Get()->GetPowerManagerClient()->
        NotifyVideoActivity(now, is_fullscreen);
    last_notify_time_ = now;
  }
}

}  // namespace chromeos
