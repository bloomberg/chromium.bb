// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_ACTIVITY_PUBLIC_ACTIVITY_VIEW_MANAGER_H_
#define ATHENA_ACTIVITY_PUBLIC_ACTIVITY_VIEW_MANAGER_H_

#include "athena/athena_export.h"

namespace athena {

class Activity;

// Manages the views for the activities.
class ATHENA_EXPORT ActivityViewManager {
 public:
  static ActivityViewManager* Create();
  static ActivityViewManager* Get();
  static void Shutdown();

  virtual ~ActivityViewManager() {}

  // Adds/Removes a task.
  virtual void AddActivity(Activity* task) = 0;
  virtual void RemoveActivity(Activity* task) = 0;

  // Updates the UI when the task color/title changes.
  virtual void UpdateActivity(Activity* task) = 0;
};

}  // namespace athena

#endif  // ATHENA_ACTIVITY_PUBLIC_ACTIVITY_VIEW_MANAGER_H_
