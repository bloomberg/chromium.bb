// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity_manager.h"

#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/views/widget/widget_observer.h"

namespace athena {

class ActivityManagerObserver;

class ActivityManagerImpl : public ActivityManager,
                            public views::WidgetObserver {
 public:
  ActivityManagerImpl();
  ~ActivityManagerImpl() override;

  int num_activities() const { return activities_.size(); }

  // ActivityManager:
  void AddActivity(Activity* activity) override;
  void RemoveActivity(Activity* activity) override;
  void UpdateActivity(Activity* activity) override;
  Activity* GetActivityForWindow(aura::Window* window) override;
  void AddObserver(ActivityManagerObserver* observer) override;
  void RemoveObserver(ActivityManagerObserver* observer) override;

  // views::WidgetObserver
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  std::vector<Activity*> activities_;

  ObserverList<ActivityManagerObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ActivityManagerImpl);
};

}  // namespace athena
