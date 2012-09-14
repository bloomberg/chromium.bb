// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_TEST_PANEL_ACTIVE_STATE_OBSERVER_H_
#define CHROME_BROWSER_UI_PANELS_TEST_PANEL_ACTIVE_STATE_OBSERVER_H_

#include "chrome/browser/ui/panels/test_panel_notification_observer.h"

class Panel;

// Custom notification observer for waiting on panel active state.
// Modeled after ui_test_utils notification observers.
class PanelActiveStateObserver : public TestPanelNotificationObserver {
 public:
  // Register to listen for panel active state change notifications
  // for the specified panel to detect a change to the expected active
  // state.
  PanelActiveStateObserver(Panel* panel, bool expect_active);
  virtual ~PanelActiveStateObserver();

 private:
  // TestNotificationObserver override:
  virtual bool AtExpectedState() OVERRIDE;

  Panel* panel_;
  bool expect_active_;

  DISALLOW_COPY_AND_ASSIGN(PanelActiveStateObserver);
};

#endif  // CHROME_BROWSER_UI_PANELS_TEST_PANEL_ACTIVE_STATE_OBSERVER_H_
