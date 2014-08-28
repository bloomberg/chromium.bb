// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/resource_manager/public/resource_manager.h"

#include <algorithm>
#include <vector>

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/activity/public/activity_manager_observer.h"
#include "athena/resource_manager/memory_pressure_notifier.h"
#include "athena/resource_manager/public/resource_manager_delegate.h"
#include "athena/wm/public/window_manager.h"
#include "athena/wm/public/window_manager_observer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window.h"

namespace athena {

class ResourceManagerImpl : public ResourceManager,
                            public WindowManagerObserver,
                            public ActivityManagerObserver,
                            public MemoryPressureObserver {
 public:
  ResourceManagerImpl(ResourceManagerDelegate* delegate);
  virtual ~ResourceManagerImpl();

  // ResourceManager:
  virtual void SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MemoryPressure pressure) OVERRIDE;

  // ActivityManagerObserver:
  virtual void OnActivityStarted(Activity* activity) OVERRIDE;
  virtual void OnActivityEnding(Activity* activity) OVERRIDE;

  // WindowManagerObserver:
  virtual void OnOverviewModeEnter() OVERRIDE;
  virtual void OnOverviewModeExit() OVERRIDE;
  virtual void OnActivityOrderHasChanged() OVERRIDE;

  // MemoryPressureObserver:
  virtual void OnMemoryPressure(
      MemoryPressureObserver::MemoryPressure pressure) OVERRIDE;
  virtual ResourceManagerDelegate* GetDelegate() OVERRIDE;

 private:
  // Manage the resources for our activities.
  void ManageResource();

  // Order our activity list to the order of activities of the stream.
  // TODO(skuhne): Once the ActivityManager is responsible to create this list
  // for us, we can remove this code here.
  void UpdateActivityOrder();

  // The sorted (new(front) -> old(back)) activity list.
  // TODO(skuhne): Once the ActivityManager is responsible to create this list
  // for us, we can remove this code here.
  std::vector<Activity*> activity_list_;

  // The resource manager delegate.
  scoped_ptr<ResourceManagerDelegate> delegate_;

  // Keeping a reference to the current memory pressure.
  MemoryPressureObserver::MemoryPressure current_memory_pressure_;

  // The memory pressure notifier.
  scoped_ptr<MemoryPressureNotifier> memory_pressure_notifier_;

  DISALLOW_COPY_AND_ASSIGN(ResourceManagerImpl);
};

namespace {
ResourceManagerImpl* instance = NULL;
}  // namespace

ResourceManagerImpl::ResourceManagerImpl(ResourceManagerDelegate* delegate)
    : delegate_(delegate),
      current_memory_pressure_(MemoryPressureObserver::MEMORY_PRESSURE_UNKNOWN),
      memory_pressure_notifier_(new MemoryPressureNotifier(this)) {
  WindowManager::GetInstance()->AddObserver(this);
  ActivityManager::Get()->AddObserver(this);
}

ResourceManagerImpl::~ResourceManagerImpl() {
  ActivityManager::Get()->RemoveObserver(this);
  WindowManager::GetInstance()->RemoveObserver(this);
}

void ResourceManagerImpl::SetMemoryPressureAndStopMonitoring(
    MemoryPressureObserver::MemoryPressure pressure) {
  memory_pressure_notifier_->StopObserving();
  OnMemoryPressure(pressure);
}

void ResourceManagerImpl::OnActivityStarted(Activity* activity) {
  // As long as we have to manage the list of activities ourselves, we need to
  // order it here.
  activity_list_.push_back(activity);
  UpdateActivityOrder();
  // Update the activity states.
  ManageResource();
}

void ResourceManagerImpl::OnActivityEnding(Activity* activity) {
  // Remove the activity from the list again.
  std::vector<Activity*>::iterator it =
      std::find(activity_list_.begin(), activity_list_.end(), activity);
  DCHECK(it != activity_list_.end());
  activity_list_.erase(it);
}

void ResourceManagerImpl::OnOverviewModeEnter() {
  // Nothing to do here.
}

void ResourceManagerImpl::OnOverviewModeExit() {
  // Nothing to do here.
}

void ResourceManagerImpl::OnActivityOrderHasChanged() {
  // As long as we have to manage the list of activities ourselves, we need to
  // order it here.
  UpdateActivityOrder();
  // Manage the resources of each activity.
  ManageResource();
}

void ResourceManagerImpl::OnMemoryPressure(
      MemoryPressureObserver::MemoryPressure pressure) {
  current_memory_pressure_ = pressure;
  ManageResource();
}

ResourceManagerDelegate* ResourceManagerImpl::GetDelegate() {
  return delegate_.get();
}

void ResourceManagerImpl::ManageResource() {
  // If there is none or only one app running we cannot do anything.
  if (activity_list_.size() <= 1U)
    return;
  // TODO(skuhne): This algorithm needs to take all kinds of predictive analysis
  // and running applications into account. For this first patch we only do a
  // very simple "floating window" algorithm which is surely not good enough.
  size_t max_running_activities = 5;
  switch (current_memory_pressure_) {
    case MEMORY_PRESSURE_UNKNOWN:
      // If we do not know how much memory we have we assume that it must be a
      // high consumption.
      // Fallthrough.
    case MEMORY_PRESSURE_HIGH:
      max_running_activities = 5;
      break;
    case MEMORY_PRESSURE_CRITICAL:
      max_running_activities = 0;
      break;
    case MEMORY_PRESSURE_MODERATE:
      max_running_activities = 7;
      break;
    case MEMORY_PRESSURE_LOW:
      // No need to do anything yet.
      return;
  }
  Activity* oldest_media_activity = NULL;
  std::vector<Activity*> unloadable_activities;
  for (std::vector<Activity*>::iterator it = activity_list_.begin();
       it != activity_list_.end(); ++it) {
    // The activity should not be unloaded or visible.
    if ((*it)->GetCurrentState() != Activity::ACTIVITY_UNLOADED &&
        !(*it)->IsVisible()) {
      if ((*it)->GetMediaState() == Activity::ACTIVITY_MEDIA_STATE_NONE) {
        // Does not play media - so we can unload this immediately.
        unloadable_activities.push_back(*it);
      } else {
        oldest_media_activity = *it;
      }
    }
  }
  if (unloadable_activities.size() > max_running_activities) {
    unloadable_activities.back()->SetCurrentState(Activity::ACTIVITY_UNLOADED);
    return;
  } else if (current_memory_pressure_ == MEMORY_PRESSURE_CRITICAL) {
    if (oldest_media_activity) {
      oldest_media_activity->SetCurrentState(Activity::ACTIVITY_UNLOADED);
      return;
    }
    LOG(ERROR) << "[ResourceManager]: Single activity uses too much memory.";
    return;
  }
  if (current_memory_pressure_ != MEMORY_PRESSURE_UNKNOWN) {
    // Only show this warning when the memory pressure is actually known. This
    // will suppress warnings in e.g. unit tests.
    LOG(WARNING) << "[ResourceManager]: No way to release memory pressure (" <<
        current_memory_pressure_ <<
        "), Activities (running, allowed, unloadable)=(" <<
        activity_list_.size() << ", " <<
        max_running_activities << ", " <<
        unloadable_activities.size() << ")";
  }
}

void ResourceManagerImpl::UpdateActivityOrder() {
  if (activity_list_.empty())
    return;
  std::vector<Activity*> new_activity_list;
  const aura::Window::Windows children =
      activity_list_[0]->GetWindow()->parent()->children();
  // Find the first window in the container which is part of the application.
  for (aura::Window::Windows::const_iterator child_iterator = children.begin();
      child_iterator != children.end(); ++child_iterator) {
    for (std::vector<Activity*>::iterator activity_iterator =
             activity_list_.begin();
        activity_iterator != activity_list_.end(); ++activity_iterator) {
      if (*child_iterator == (*activity_iterator)->GetWindow()) {
        new_activity_list.push_back(*activity_iterator);
        activity_list_.erase(activity_iterator);
        break;
      }
    }
  }
  // At this point the old list should be empty and we can swap the lists.
  DCHECK(!activity_list_.size());
  activity_list_ = new_activity_list;
}

// static
void ResourceManager::Create() {
  DCHECK(!instance);
  instance = new ResourceManagerImpl(
      ResourceManagerDelegate::CreateResourceManagerDelegate());
}

// static
ResourceManager* ResourceManager::Get() {
  return instance;
}

// static
void ResourceManager::Shutdown() {
  DCHECK(instance);
  delete instance;
  instance = NULL;
}

ResourceManager::ResourceManager() {}

ResourceManager::~ResourceManager() {
  DCHECK(instance);
  instance = NULL;
}

}  // namespace athena
