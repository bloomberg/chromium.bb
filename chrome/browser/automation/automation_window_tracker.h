// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_AUTOMATION_WINDOW_TRACKER_H_
#define CHROME_BROWSER_AUTOMATION_AUTOMATION_WINDOW_TRACKER_H_
#pragma once

#include "build/build_config.h"
#include "chrome/browser/automation/automation_resource_tracker.h"
#include "ui/gfx/native_widget_types.h"

class AutomationWindowTracker
    : public AutomationResourceTracker<gfx::NativeWindow> {
 public:
  explicit AutomationWindowTracker(IPC::Message::Sender* automation);
  virtual ~AutomationWindowTracker();

  virtual void AddObserver(gfx::NativeWindow resource);
  virtual void RemoveObserver(gfx::NativeWindow resource);
};

#endif  // CHROME_BROWSER_AUTOMATION_AUTOMATION_WINDOW_TRACKER_H_
