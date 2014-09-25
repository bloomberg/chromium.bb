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
  virtual ~ActivityManagerImpl();

  int num_activities() const { return activities_.size(); }

  // ActivityManager:
  virtual void AddActivity(Activity* activity) OVERRIDE;
  virtual void RemoveActivity(Activity* activity) OVERRIDE;
  virtual void UpdateActivity(Activity* activity) OVERRIDE;
  virtual Activity* GetActivityForWindow(aura::Window* window) OVERRIDE;
  virtual void AddObserver(ActivityManagerObserver* observer) OVERRIDE;
  virtual void RemoveObserver(ActivityManagerObserver* observer) OVERRIDE;

  // views::WidgetObserver
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

 private:
  std::vector<Activity*> activities_;

  ObserverList<ActivityManagerObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ActivityManagerImpl);
};

}  // namespace athena
