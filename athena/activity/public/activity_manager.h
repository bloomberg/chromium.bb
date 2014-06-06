// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_ACTIVITY_PUBLIC_ACTIVITY_MANAGER_H_
#define ATHENA_ACTIVITY_PUBLIC_ACTIVITY_MANAGER_H_

#include "athena/athena_export.h"

namespace athena {

class Activity;

// Manages a set of activities.
class ATHENA_EXPORT ActivityManager {
 public:
  static ActivityManager* Create();
  static ActivityManager* Get();
  static void Shutdown();

  virtual ~ActivityManager() {}

  // Adds/Removes an activity. ActivityManager owns the activities.
  virtual void AddActivity(Activity* activity) = 0;
  virtual void RemoveActivity(Activity* activity) = 0;

  // Updates the UI when the task color/title changes.
  virtual void UpdateActivity(Activity* activity) = 0;
};

}  // namespace athena

#endif  // ATHENA_ACTIVITY_PUBLIC_ACTIVITY_MANAGER_H_
