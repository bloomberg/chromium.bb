// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/idle_detector.h"

#include "base/bind.h"
#include "base/logging.h"
#include "ui/wm/core/user_activity_detector.h"

namespace chromeos {

IdleDetector::IdleDetector(const base::Closure& on_active_callback,
                           const base::Closure& on_idle_callback)
    : active_callback_(on_active_callback), idle_callback_(on_idle_callback) {}

IdleDetector::~IdleDetector() {
  wm::UserActivityDetector* user_activity_detector =
      wm::UserActivityDetector::Get();
  if (user_activity_detector && user_activity_detector->HasObserver(this))
    user_activity_detector->RemoveObserver(this);
}

void IdleDetector::OnUserActivity(const ui::Event* event) {
  if (!active_callback_.is_null())
    active_callback_.Run();
  ResetTimer();
}

void IdleDetector::Start(const base::TimeDelta& timeout) {
  timeout_ = timeout;
  if (!wm::UserActivityDetector::Get()->HasObserver(this))
    wm::UserActivityDetector::Get()->AddObserver(this);
  ResetTimer();
}

void IdleDetector::ResetTimer() {
  if (timer_.IsRunning())
    timer_.Reset();
  else
    timer_.Start(FROM_HERE, timeout_, idle_callback_);
}

}  // namespace chromeos
