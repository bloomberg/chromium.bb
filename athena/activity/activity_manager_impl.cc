// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/activity_manager_impl.h"

#include <algorithm>

#include "athena/activity/activity_widget_delegate.h"
#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_manager_observer.h"
#include "athena/activity/public/activity_view_model.h"
#include "athena/screen/public/screen_manager.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

namespace athena {

namespace {

ActivityManager* instance = nullptr;

views::Widget* CreateWidget(Activity* activity) {
  aura::Window* window = activity->GetWindow();
  views::Widget* widget =
      window ? views::Widget::GetWidgetForNativeView(window) : nullptr;
  if (widget)
    return widget;
  widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate =
      new ActivityWidgetDelegate(activity->GetActivityViewModel());
  widget->Init(params);
  return widget;
}

views::Widget* GetWidget(Activity* activity) {
  CHECK(activity);
  CHECK(activity->GetWindow());
  return views::Widget::GetWidgetForNativeWindow(activity->GetWindow());
}

}  // namespace

ActivityManagerImpl::ActivityManagerImpl() {
  CHECK(!instance);
  instance = this;

  aura::Window* root_window =
      ScreenManager::Get()->GetContext()->GetRootWindow();
  aura::client::GetActivationClient(root_window)->AddObserver(this);
}

ActivityManagerImpl::~ActivityManagerImpl() {
  aura::Window* root_window =
      ScreenManager::Get()->GetContext()->GetRootWindow();
  aura::client::GetActivationClient(root_window)->RemoveObserver(this);
  while (!activities_.empty())
    Activity::Delete(activities_.front());

  CHECK_EQ(this, instance);
  instance = nullptr;
}

void ActivityManagerImpl::AddActivity(Activity* activity) {
  CHECK(activities_.end() ==
        std::find(activities_.begin(), activities_.end(), activity));
  activities_.insert(activities_.begin(), activity);
  views::Widget* widget = CreateWidget(activity);
  widget->GetNativeView()->AddObserver(this);
  activity->GetActivityViewModel()->Init();

  FOR_EACH_OBSERVER(ActivityManagerObserver,
                    observers_,
                    OnActivityStarted(activity));
}

void ActivityManagerImpl::RemoveActivity(Activity* activity) {
  std::vector<Activity*>::iterator find =
      std::find(activities_.begin(), activities_.end(), activity);
  DCHECK(find != activities_.end());
  if (find != activities_.end()) {
    FOR_EACH_OBSERVER(
        ActivityManagerObserver, observers_, OnActivityEnding(activity));
    activities_.erase(find);
    views::Widget* widget = GetWidget(activity);
    widget->GetNativeView()->RemoveObserver(this);
    widget->Close();
  }
}

const ActivityList& ActivityManagerImpl::GetActivityList() {
  return activities_;
}

Activity* ActivityManagerImpl::GetActivityForWindow(aura::Window* window) {
  struct Matcher {
    Matcher(aura::Window* w) : window(w) {}
    bool operator()(Activity* activity) {
      return activity->GetWindow() == window;
    }
    aura::Window* window;
  };
  std::vector<Activity*>::iterator iter =
      std::find_if(activities_.begin(), activities_.end(), Matcher(window));
  return iter != activities_.end() ? *iter : nullptr;
}

void ActivityManagerImpl::AddObserver(ActivityManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void ActivityManagerImpl::RemoveObserver(ActivityManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ActivityManagerImpl::OnWindowDestroying(aura::Window* window) {
  Activity* activity = GetActivityForWindow(window);
  if (activity) {
    window->RemoveObserver(this);
    Activity::Delete(activity);
  }
}

void ActivityManagerImpl::OnWindowActivated(aura::Window* gained_active,
                                            aura::Window* lost_active) {
  Activity* activity = GetActivityForWindow(gained_active);
  if (!activity)
    return;
  CHECK(!activities_.empty());
  if (activity == activities_.front())
    return;
  // Move the activity for |gained_active| at the front of the list.
  ActivityList::reverse_iterator iter = std::find(activities_.rbegin(),
                                                  activities_.rend(),
                                                  activity);
  CHECK(iter != activities_.rend());
  std::rotate(iter, iter + 1, activities_.rend());
  FOR_EACH_OBSERVER(ActivityManagerObserver,
                    observers_,
                    OnActivityOrderChanged());
}

// static
ActivityManager* ActivityManager::Create() {
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
}


}  // namespace athena
