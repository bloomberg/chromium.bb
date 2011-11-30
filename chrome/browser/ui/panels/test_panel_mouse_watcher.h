// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_TEST_PANEL_MOUSE_WATCHER_H_
#define CHROME_BROWSER_UI_PANELS_TEST_PANEL_MOUSE_WATCHER_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"

// Test mouse watcher for simulating mouse movements in tests.
class TestPanelMouseWatcher : public PanelMouseWatcher {
 public:
  TestPanelMouseWatcher();
  virtual ~TestPanelMouseWatcher();

 private:
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;

  bool started_;
  DISALLOW_COPY_AND_ASSIGN(TestPanelMouseWatcher);
};

#endif  // CHROME_BROWSER_UI_PANELS_TEST_PANEL_MOUSE_WATCHER_H_
