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
  ~ActivityLifetimeTracker() override;

  // ActivityManagerObserver:
  void OnActivityStarted(Activity* activity) override;
  void OnActivityEnding(Activity* activity) override;
  void OnActivityOrderChanged() override;

  // Returns the newly created activity (if any) and resets its reference.
  Activity* GetNewActivityAndReset();

  // Returns a deleted activity (if any) and resets its reference.
  // Note that the pointer is void* since the Activity was deleted when
  // requested.
  void* GetDeletedActivityAndReset();

 private:
  // The activity which got created if not nullptr.
  Activity* new_activity_;

  // An old activity which got unloaded if not nullptr.
  void* deleted_activity_;

  DISALLOW_COPY_AND_ASSIGN(ActivityLifetimeTracker);
};

}  // namespace athena

#endif //  ATHENA_TEST_BASE_ACTIVITY_LIFETIME_TRACKER_H_

