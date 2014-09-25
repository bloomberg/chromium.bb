// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/activity_manager_impl.h"

#include <algorithm>

#include "athena/activity/activity_widget_delegate.h"
#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_manager_observer.h"
#include "athena/activity/public/activity_view_model.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "ui/views/widget/widget.h"

namespace athena {

namespace {

ActivityManager* instance = NULL;

views::Widget* CreateWidget(Activity* activity) {
  ActivityViewModel* view_model = activity->GetActivityViewModel();
  views::Widget* widget = view_model->CreateWidget();
  if (widget)
    return widget;
  widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = new ActivityWidgetDelegate(view_model);
  widget->Init(params);
  activity->GetActivityViewModel()->Init();
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
}

ActivityManagerImpl::~ActivityManagerImpl() {
  while (!activities_.empty())
    Activity::Delete(activities_.front());

  CHECK_EQ(this, instance);
  instance = NULL;
}

void ActivityManagerImpl::AddActivity(Activity* activity) {
  CHECK(activities_.end() ==
        std::find(activities_.begin(), activities_.end(), activity));
  activities_.push_back(activity);
  views::Widget* widget = CreateWidget(activity);
  widget->AddObserver(this);
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
    widget->RemoveObserver(this);
    widget->Close();
  }
}

void ActivityManagerImpl::UpdateActivity(Activity* activity) {
  views::Widget* widget = GetWidget(activity);
  widget->UpdateWindowIcon();
  widget->UpdateWindowTitle();
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
  return iter != activities_.end() ? *iter : NULL;
}

void ActivityManagerImpl::AddObserver(ActivityManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void ActivityManagerImpl::RemoveObserver(ActivityManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ActivityManagerImpl::OnWidgetDestroying(views::Widget* widget) {
  Activity* activity = GetActivityForWindow(widget->GetNativeWindow());
  if (activity) {
    widget->RemoveObserver(this);
    Activity::Delete(activity);
  }
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
