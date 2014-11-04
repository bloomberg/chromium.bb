// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/test/base/activity_lifetime_tracker.h"

#include "athena/activity/public/activity_manager.h"

namespace athena {

ActivityLifetimeTracker::ActivityLifetimeTracker()
    : new_activity_(nullptr), deleted_activity_(nullptr) {
  ActivityManager::Get()->AddObserver(this);
}

ActivityLifetimeTracker::~ActivityLifetimeTracker() {
  ActivityManager::Get()->RemoveObserver(this);
}

void ActivityLifetimeTracker::OnActivityStarted(Activity* activity) {
  new_activity_ = activity;
}

void ActivityLifetimeTracker::OnActivityEnding(Activity* activity) {
  deleted_activity_ = activity;
}

void ActivityLifetimeTracker::OnActivityOrderChanged() {
}

Activity* ActivityLifetimeTracker::GetNewActivityAndReset() {
  Activity* activity = new_activity_;
  new_activity_ = nullptr;
  return activity;
}

void* ActivityLifetimeTracker::GetDeletedActivityAndReset() {
  void* activity = deleted_activity_;
  deleted_activity_ = nullptr;
  return activity;
}

}  // namespace athena
