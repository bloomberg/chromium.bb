// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_BROWSER_TRACKER_H__
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_BROWSER_TRACKER_H__
#pragma once

#include "chrome/browser/automation/automation_resource_tracker.h"

class Browser;

// Tracks Browser objects.
class AutomationBrowserTracker : public AutomationResourceTracker<Browser*> {
 public:
  explicit AutomationBrowserTracker(IPC::Message::Sender* automation);
  virtual ~AutomationBrowserTracker();
  virtual void AddObserver(Browser* resource);
  virtual void RemoveObserver(Browser* resource);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_BROWSER_TRACKER_H__
