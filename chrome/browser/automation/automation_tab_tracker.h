// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_TAB_TRACKER_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_TAB_TRACKER_H_
#pragma once

#include <map>

#include "base/time.h"
#include "chrome/browser/automation/automation_resource_tracker.h"

class NavigationController;
class NotificationType;

class AutomationTabTracker
  : public AutomationResourceTracker<NavigationController*> {
 public:
  explicit AutomationTabTracker(IPC::Message::Sender* automation);
  virtual ~AutomationTabTracker();

  virtual void AddObserver(NavigationController* resource);
  virtual void RemoveObserver(NavigationController* resource);

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  base::Time GetLastNavigationTime(int handle);

 private:
  // Last time a navigation occurred.
  std::map<NavigationController*, base::Time> last_navigation_times_;

  DISALLOW_COPY_AND_ASSIGN(AutomationTabTracker);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_TAB_TRACKER_H_
