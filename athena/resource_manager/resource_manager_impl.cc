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
#include "athena/wm/public/window_list_provider.h"
#include "athena/wm/public/window_list_provider_observer.h"
#include "athena/wm/public/window_manager.h"
#include "athena/wm/public/window_manager_observer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "ui/aura/window.h"

namespace athena {

class ResourceManagerImpl : public ResourceManager,
                            public WindowManagerObserver,
                            public ActivityManagerObserver,
                            public MemoryPressureObserver,
                            public WindowListProviderObserver {
 public:
  ResourceManagerImpl(ResourceManagerDelegate* delegate);
  virtual ~ResourceManagerImpl();

  // ResourceManager:
  virtual void SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MemoryPressure pressure) OVERRIDE;
  virtual void SetWaitTimeBetweenResourceManageCalls(int time_in_ms) OVERRIDE {
    wait_time_for_resource_deallocation_ =
        base::TimeDelta::FromMilliseconds(time_in_ms);
    // Reset the timeout to force the next resource call to execute immediately.
    next_resource_management_time_ = base::Time::Now();
  }

  virtual void Pause(bool pause) OVERRIDE {
    if (pause) {
      if (!pause_)
        queued_command_ = false;
      ++pause_;
    } else {
      DCHECK(pause_);
      --pause_;
      if (!pause && queued_command_) {
        UpdateActivityOrder();
        ManageResource();
      }
    }
  }

  // ActivityManagerObserver:
  virtual void OnActivityStarted(Activity* activity) OVERRIDE;
  virtual void OnActivityEnding(Activity* activity) OVERRIDE;

  // WindowManagerObserver:
  virtual void OnOverviewModeEnter() OVERRIDE;
  virtual void OnOverviewModeExit() OVERRIDE;
  virtual void OnSplitViewModeEnter() OVERRIDE;
  virtual void OnSplitViewModeExit() OVERRIDE;

  // MemoryPressureObserver:
  virtual void OnMemoryPressure(
      MemoryPressureObserver::MemoryPressure pressure) OVERRIDE;
  virtual ResourceManagerDelegate* GetDelegate() OVERRIDE;

  // WindowListProviderObserver:
  virtual void OnWindowStackingChanged() OVERRIDE;
  virtual void OnWindowRemoved(aura::Window* removed_window,
                               int index) OVERRIDE;

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

  // A ref counter. As long as not 0, the management is on hold.
  int pause_;

  // If true, a command came in while the resource manager was paused.
  bool queued_command_;

  // Used by ManageResource() to determine an activity state change while it
  // changes Activity properties.
  bool activity_order_changed_;

  // True if in overview mode - activity order changes will be ignored if true
  // and postponed till after the overview mode is ending.
  bool in_overview_mode_;

  // True if we are in split view mode.
  bool in_splitview_mode_;

  // The last time the resource manager was called to release resources.
  // Avoid too aggressive resource de-allocation by enforcing a wait time of
  // |wait_time_for_resource_deallocation_| between executed calls.
  base::Time next_resource_management_time_;

  // The wait time between two resource managing executions.
  base::TimeDelta wait_time_for_resource_deallocation_;

  DISALLOW_COPY_AND_ASSIGN(ResourceManagerImpl);
};

namespace {
ResourceManagerImpl* instance = NULL;

// We allow this many activities to be visible. All others must be at state of
// invisible or below.
const int kMaxVisibleActivities = 3;

}  // namespace

ResourceManagerImpl::ResourceManagerImpl(ResourceManagerDelegate* delegate)
    : delegate_(delegate),
      current_memory_pressure_(MemoryPressureObserver::MEMORY_PRESSURE_UNKNOWN),
      memory_pressure_notifier_(new MemoryPressureNotifier(this)),
      pause_(false),
      queued_command_(false),
      activity_order_changed_(false),
      in_overview_mode_(false),
      in_splitview_mode_(false),
      next_resource_management_time_(base::Time::Now()),
      wait_time_for_resource_deallocation_(base::TimeDelta::FromMilliseconds(
          delegate_->MemoryPressureIntervalInMS())) {
  WindowManager::GetInstance()->AddObserver(this);
  WindowManager::GetInstance()->GetWindowListProvider()->AddObserver(this);
  ActivityManager::Get()->AddObserver(this);
}

ResourceManagerImpl::~ResourceManagerImpl() {
  ActivityManager::Get()->RemoveObserver(this);
  WindowManager::GetInstance()->GetWindowListProvider()->RemoveObserver(this);
  WindowManager::GetInstance()->RemoveObserver(this);

  while (!activity_list_.empty())
    OnActivityEnding(activity_list_.front());
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
  // Remember that the activity order has changed.
  activity_order_changed_ = true;
}

void ResourceManagerImpl::OnActivityEnding(Activity* activity) {
  DCHECK(activity->GetWindow());
  // Remove the activity from the list again.
  std::vector<Activity*>::iterator it =
      std::find(activity_list_.begin(), activity_list_.end(), activity);
  DCHECK(it != activity_list_.end());
  activity_list_.erase(it);
  // Remember that the activity order has changed.
  activity_order_changed_ = true;
}

void ResourceManagerImpl::OnOverviewModeEnter() {
  in_overview_mode_ = true;
}

void ResourceManagerImpl::OnOverviewModeExit() {
  in_overview_mode_ = false;
  // Reorder the activities and manage the resources again since an order change
  // might have caused a visibility change.
  UpdateActivityOrder();
  ManageResource();
}

void ResourceManagerImpl::OnSplitViewModeEnter() {
  // Re-apply the memory pressure to make sure enough items are visible.
  in_splitview_mode_ = true;
  ManageResource();
}


void ResourceManagerImpl::OnSplitViewModeExit() {
  // We don't do immediately something yet. The next ManageResource call will
  // come soon.
  in_splitview_mode_ = false;
}

void ResourceManagerImpl::OnWindowStackingChanged() {
  activity_order_changed_ = true;
  if (pause_) {
    queued_command_ = true;
    return;
  }

  // No need to do anything while being in overview mode.
  if (in_overview_mode_)
    return;

  // As long as we have to manage the list of activities ourselves, we need to
  // order it here.
  UpdateActivityOrder();

  // Manage the resources of each activity.
  ManageResource();
}

void ResourceManagerImpl::OnWindowRemoved(aura::Window* removed_window,
                                          int index) {
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

  if (pause_) {
    queued_command_ = true;
    return;
  }

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
      // This doesn't really matter. We do not change anything but turning
      // activities visible.
      max_running_activities = 10000;
      break;
  }

  // The first n activities should be treated as "visible", means they updated
  // in overview mode and will keep their layer resources for faster switch
  // times. Usually we use |kMaxVisibleActivities| items, but when the memory
  // pressure gets critical we only hold as many as are really visible.
  size_t max_activities = kMaxVisibleActivities;
  if (current_memory_pressure_ == MEMORY_PRESSURE_CRITICAL)
    max_activities = 1 + (in_splitview_mode_ ? 1 : 0);

  // Restart and / or bail if the order of activities changes due to our calls.
  activity_order_changed_ = false;

  // Change the visibility of our activities in a pre-processing step. This is
  // required since it might change the order/number of activities.
  size_t index = 0;
  while (index < activity_list_.size()) {
    Activity* activity = activity_list_[index];
    Activity::ActivityState state = activity->GetCurrentState();

    // The first |kMaxVisibleActivities| entries should be visible, all others
    // invisible or at a lower activity state.
    if (index < max_activities ||
        (state == Activity::ACTIVITY_INVISIBLE ||
         state == Activity::ACTIVITY_VISIBLE)) {
      Activity::ActivityState visiblity_state =
          index < max_activities ? Activity::ACTIVITY_VISIBLE :
                                   Activity::ACTIVITY_INVISIBLE;
      // Only change the state when it changes. Note that when the memory
      // pressure is critical, only the primary activities (1 or 2) are made
      // visible. Furthermore, in relaxed mode we only want to turn visible,
      // never invisible.
      if (visiblity_state != state &&
          (current_memory_pressure_ != MEMORY_PRESSURE_LOW ||
           visiblity_state == Activity::ACTIVITY_VISIBLE)) {
        activity->SetCurrentState(visiblity_state);
        // If we turned an activity invisible, we should not at the same time
        // throw an activity out of memory. Thus we grant one more invisible
        // Activity in that case.
        if (visiblity_state == Activity::ACTIVITY_INVISIBLE)
          max_running_activities++;
      }
    }

    // See which index we should handle next.
    if (activity_order_changed_) {
      activity_order_changed_ = false;
      index = 0;
    } else {
      ++index;
    }
  }

  // If there is only a low memory pressure, or our last call to release
  // resources cannot have had any impact yet, we return.
  // TODO(skuhne): The upper part of this function bumps up the state (to
  // visible) and the lower part might unload one. Going forward this algorithm
  // will change significantly and when it does we might want to break this into
  // two separate pieces.
  if (current_memory_pressure_ == MEMORY_PRESSURE_LOW ||
      base::Time::Now() < next_resource_management_time_)
    return;
  // Do not release too many activities in short succession since it takes time
  // to release resources. As such wait the memory pressure interval before the
  // next call.
  next_resource_management_time_ = base::Time::Now() +
                                   wait_time_for_resource_deallocation_;

  // Check if/which activity we want to unload.
  Activity* oldest_media_activity = NULL;
  std::vector<Activity*> unloadable_activities;
  for (std::vector<Activity*>::iterator it = activity_list_.begin();
       it != activity_list_.end(); ++it) {
    Activity::ActivityState state = (*it)->GetCurrentState();
    // The activity should neither be unloaded nor visible.
    if (state != Activity::ACTIVITY_UNLOADED &&
        state != Activity::ACTIVITY_VISIBLE) {
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
      LOG(WARNING) << "Unloading item to releave critical memory pressure";
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
  queued_command_ = true;
  if (activity_list_.empty())
    return;
  std::vector<Activity*> new_activity_list;
  const aura::Window::Windows children =
      activity_list_[0]->GetWindow()->parent()->children();
  // Find the first window in the container which is part of the application.
  for (aura::Window::Windows::const_reverse_iterator child_iterator =
         children.rbegin();
      child_iterator != children.rend(); ++child_iterator) {
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

  // Remember that the activity order has changed.
  activity_order_changed_ = true;
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
