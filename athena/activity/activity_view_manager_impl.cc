// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity_view_manager.h"

#include <algorithm>
#include <map>

#include "athena/activity/activity_widget_delegate.h"
#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_view_model.h"
#include "athena/screen/public/screen_manager.h"
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace athena {
namespace {

class ActivityWidget {
 public:
  explicit ActivityWidget(Activity* activity)
      : activity_(activity), widget_(NULL) {
    ActivityViewModel* view_model = activity->GetActivityViewModel();
    widget_ = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.context = ScreenManager::Get()->GetContext();
    params.delegate = new ActivityWidgetDelegate(view_model);
    params.activatable = views::Widget::InitParams::ACTIVATABLE_YES;
    widget_->Init(params);
    activity_->GetActivityViewModel()->Init();
  }

  virtual ~ActivityWidget() {
    widget_->Close();
  }

  void Show() {
    Update();
    widget_->Show();
    widget_->Activate();
  }

  void Update() {
    widget_->UpdateWindowTitle();
  }

 private:

  Activity* activity_;
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(ActivityWidget);
};

ActivityViewManager* instance = NULL;

class ActivityViewManagerImpl : public ActivityViewManager {
 public:
  ActivityViewManagerImpl() {
    CHECK(!instance);
    instance = this;
  }
  virtual ~ActivityViewManagerImpl() {
    CHECK_EQ(this, instance);
    instance = NULL;
  }

  // ActivityViewManager:
  virtual void AddActivity(Activity* activity) OVERRIDE {
    CHECK(activity_widgets_.end() == activity_widgets_.find(activity));
    ActivityWidget* container = new ActivityWidget(activity);
    activity_widgets_[activity] = container;
    container->Show();
  }

  virtual void RemoveActivity(Activity* activity) OVERRIDE {
    std::map<Activity*, ActivityWidget*>::iterator find =
        activity_widgets_.find(activity);
    if (find != activity_widgets_.end()) {
      ActivityWidget* widget = find->second;
      activity_widgets_.erase(activity);
      delete widget;
    }
  }

  virtual void UpdateActivity(Activity* activity) OVERRIDE {
    std::map<Activity*, ActivityWidget*>::iterator find =
        activity_widgets_.find(activity);
    if (find != activity_widgets_.end())
      find->second->Update();
  }

 private:
  std::map<Activity*, ActivityWidget*> activity_widgets_;

  DISALLOW_COPY_AND_ASSIGN(ActivityViewManagerImpl);
};

}  // namespace

// static
ActivityViewManager* ActivityViewManager::Create() {
  new ActivityViewManagerImpl();
  CHECK(instance);
  return instance;
}

ActivityViewManager* ActivityViewManager::Get() {
  return instance;
}

void ActivityViewManager::Shutdown() {
  CHECK(instance);
  delete instance;
}

}  // namespace athena
