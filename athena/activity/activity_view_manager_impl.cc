// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity_view_manager.h"

#include <algorithm>
#include <map>

#include "athena/activity/activity_widget_delegate.h"
#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/activity/public/activity_view_model.h"
#include "athena/screen/public/screen_manager.h"
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace athena {
namespace {

typedef std::map<Activity*, views::Widget*> ActivityWidgetMap;

views::Widget* CreateWidget(Activity* activity) {
  ActivityViewModel* view_model = activity->GetActivityViewModel();
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = new ActivityWidgetDelegate(view_model);
  params.activatable = views::Widget::InitParams::ACTIVATABLE_YES;
  widget->Init(params);
  return widget;
}

ActivityViewManager* instance = NULL;

class ActivityViewManagerImpl : public ActivityViewManager,
                                public views::WidgetObserver {
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
    views::Widget* container = CreateWidget(activity);
    container->AddObserver(this);
    activity_widgets_[activity] = container;
    container->UpdateWindowTitle();
    container->Show();
    container->Activate();
    // Call the Activity model's initializer. It might re-order the activity
    // against others, which has to be done before before registering it to the
    // system.
    activity->GetActivityViewModel()->Init();
  }

  virtual void RemoveActivity(Activity* activity) OVERRIDE {
    ActivityWidgetMap::iterator find = activity_widgets_.find(activity);
    if (find != activity_widgets_.end()) {
      views::Widget* widget = find->second;
      widget->RemoveObserver(this);
      widget->Close();
      activity_widgets_.erase(activity);
    }
  }

  virtual void UpdateActivity(Activity* activity) OVERRIDE {
    ActivityWidgetMap::iterator find = activity_widgets_.find(activity);
    if (find != activity_widgets_.end())
      find->second->UpdateWindowTitle();
  }

  // views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE {
    for (ActivityWidgetMap::iterator iter = activity_widgets_.begin();
         iter != activity_widgets_.end();
         ++iter) {
      if (iter->second == widget) {
        Activity::Delete(iter->first);
        break;
      }
    }
  }

 private:
  ActivityWidgetMap activity_widgets_;

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
