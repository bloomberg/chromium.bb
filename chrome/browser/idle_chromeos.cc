// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/idle.h"

#include "ash/shell.h"
#include "ash/wm/user_activity_detector.h"
#include "base/time.h"

void CalculateIdleTime(IdleTimeCallback notify) {
  base::TimeDelta idle_time = base::TimeTicks::Now() -
      ash::Shell::GetInstance()->user_activity_detector()->last_activity_time();
  notify.Run(static_cast<int>(idle_time.InSeconds()));
}

bool CheckIdleStateIsLocked() {
  return ash::Shell::GetInstance()->IsScreenLocked();
}
