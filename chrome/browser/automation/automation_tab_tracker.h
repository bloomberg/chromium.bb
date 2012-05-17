// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_TAB_TRACKER_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_TAB_TRACKER_H_
#pragma once

#include <map>

#include "base/time.h"
#include "chrome/browser/automation/automation_resource_tracker.h"

namespace content {
class NavigationController;
}

class AutomationTabTracker
  : public AutomationResourceTracker<content::NavigationController*> {
 public:
  explicit AutomationTabTracker(IPC::Message::Sender* automation);
  virtual ~AutomationTabTracker();

  virtual void AddObserver(content::NavigationController* resource);
  virtual void RemoveObserver(content::NavigationController* resource);

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

 private:
  DISALLOW_COPY_AND_ASSIGN(AutomationTabTracker);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_TAB_TRACKER_H_
