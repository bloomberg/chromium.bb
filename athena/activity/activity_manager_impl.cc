// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity_manager.h"

#include <algorithm>
#include <vector>

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_view_manager.h"
#include "base/logging.h"

namespace athena {

namespace {

ActivityManager* instance = NULL;

class ActivityManagerImpl : public ActivityManager {
 public:
  ActivityManagerImpl() {
    CHECK(!instance);
    instance = this;
  }
  virtual ~ActivityManagerImpl() {
    while (!activities_.empty())
      delete activities_.front();

    CHECK_EQ(this, instance);
    instance = NULL;
  }

  // ActivityManager:
  virtual void AddActivity(Activity* activity) OVERRIDE {
    CHECK(activities_.end() == std::find(activities_.begin(),
                                         activities_.end(),
                                         activity));
    activities_.push_back(activity);
    ActivityViewManager* manager = ActivityViewManager::Get();
    manager->AddActivity(activity);
  }

  virtual void RemoveActivity(Activity* activity) OVERRIDE {
    std::vector<Activity*>::iterator find = std::find(activities_.begin(),
                                                      activities_.end(),
                                                      activity);
    if (find != activities_.end()) {
      activities_.erase(find);

      ActivityViewManager* manager = ActivityViewManager::Get();
      manager->RemoveActivity(activity);
    }
  }

  virtual void UpdateActivity(Activity* activity) OVERRIDE {
    ActivityViewManager* manager = ActivityViewManager::Get();
    manager->UpdateActivity(activity);
  }

 private:
  std::vector<Activity*> activities_;

  DISALLOW_COPY_AND_ASSIGN(ActivityManagerImpl);
};

}  // namespace

// static
ActivityManager* ActivityManager::Create() {
  ActivityViewManager::Create();

  new ActivityManagerImpl();
  CHECK(instance);
  return instance;
}

ActivityManager* ActivityManager::Get() {
  return instance;
}

void ActivityManager::Shutdown() {
  CHECK(instance);
  delete instance;
  ActivityViewManager::Shutdown();
}

}  // namespace athena
