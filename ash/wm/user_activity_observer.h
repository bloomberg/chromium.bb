// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_USER_ACTIVITY_OBSERVER_H_
#define ASH_WM_USER_ACTIVITY_OBSERVER_H_
#pragma once

#include "ash/ash_export.h"
#include "base/basictypes.h"

namespace ash {

// Interface for classes that want to be notified about user activity.
// Implementations should register themselves with UserActivityDetector.
class ASH_EXPORT UserActivityObserver {
 public:
  // Invoked periodically while the user is active (i.e. generating input
  // events).
  virtual void OnUserActivity() = 0;

 protected:
  UserActivityObserver() {}
  virtual ~UserActivityObserver() {}

  DISALLOW_COPY_AND_ASSIGN(UserActivityObserver);
};

}  // namespace ash

#endif  // ASH_WM_USER_ACTIVITY_OBSERVER_H_
