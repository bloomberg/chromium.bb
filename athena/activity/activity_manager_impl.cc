// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/activity_manager_impl.h"

#include <algorithm>

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_manager_observer.h"
#include "athena/activity/public/activity_view_manager.h"
#include "base/logging.h"
#include "base/observer_list.h"

namespace athena {

namespace {

ActivityManager* instance = NULL;

}  // namespace

ActivityManagerImpl::ActivityManagerImpl() {
  CHECK(!instance);
  instance = this;
}

ActivityManagerImpl::~ActivityManagerImpl() {
  while (!activities_.empty())
    delete activities_.front();

  CHECK_EQ(this, instance);
  instance = NULL;
}

void ActivityManagerImpl::AddActivity(Activity* activity) {
  CHECK(activities_.end() ==
        std::find(activities_.begin(), activities_.end(), activity));
  activities_.push_back(activity);
  ActivityViewManager* manager = ActivityViewManager::Get();
  manager->AddActivity(activity);
  FOR_EACH_OBSERVER(ActivityManagerObserver,
                    observers_,
                    OnActivityStarted(activity));
}

void ActivityManagerImpl::RemoveActivity(Activity* activity) {
  std::vector<Activity*>::iterator find =
      std::find(activities_.begin(), activities_.end(), activity);
  FOR_EACH_OBSERVER(ActivityManagerObserver,
                    observers_,
                    OnActivityEnding(activity));
  if (find != activities_.end()) {
    activities_.erase(find);
    ActivityViewManager* manager = ActivityViewManager::Get();
    manager->RemoveActivity(activity);
  }
}

void ActivityManagerImpl::UpdateActivity(Activity* activity) {
  ActivityViewManager* manager = ActivityViewManager::Get();
  manager->UpdateActivity(activity);
}

void ActivityManagerImpl::AddObserver(ActivityManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void ActivityManagerImpl::RemoveObserver(ActivityManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

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
