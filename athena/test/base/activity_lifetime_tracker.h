// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_TEST_BASE_ACTIVITY_LIFETIME_TRACKER_H_
#define ATHENA_TEST_BASE_ACTIVITY_LIFETIME_TRACKER_H_

#include "athena/activity/public/activity_manager_observer.h"
#include "base/macros.h"

namespace athena {
class Activity;

// Listens for activities to be started / ended and will return them.
class ActivityLifetimeTracker : public ActivityManagerObserver {
 public:
  ActivityLifetimeTracker();
  virtual ~ActivityLifetimeTracker();

  // Called after an |activity| got created.
  virtual void OnActivityStarted(Activity* activity) OVERRIDE;

  // Called before an |activity| gets destroyed.
  virtual void OnActivityEnding(Activity* activity) OVERRIDE;

  // Returns the newly created activity (if any) and resets its reference.
  Activity* GetNewActivityAndReset();

  // Returns a deleted activity (if any) and resets its reference.
  // Note that the pointer is void* since the Activity was deleted when
  // requested.
  void* GetDeletedActivityAndReset();

 private:
  // The activity which got created if not NULL.
  Activity* new_activity_;

  // An old activity which got unloaded if not NULL.
  void* deleted_activity_;

  DISALLOW_COPY_AND_ASSIGN(ActivityLifetimeTracker);
};

}  // namespace athena

#endif //  ATHENA_TEST_BASE_ACTIVITY_LIFETIME_TRACKER_H_

