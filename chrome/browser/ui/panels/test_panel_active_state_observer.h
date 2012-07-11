// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_TEST_PANEL_ACTIVE_STATE_OBSERVER_H_
#define CHROME_BROWSER_UI_PANELS_TEST_PANEL_ACTIVE_STATE_OBSERVER_H_

#include "chrome/test/base/ui_test_utils.h"

class Panel;

// Custom notification observer for waiting on panel active state.
// Modeled after ui_test_utils notification observers.
class PanelActiveStateObserver : public content::NotificationObserver {
 public:
  // Register to listen for panel active state change notifications
  // for the specified panel to detect a change to the expected active
  // state.
  PanelActiveStateObserver(Panel* panel, bool expect_active);
  virtual ~PanelActiveStateObserver();

  // Wait until the active state has changed to the expected state.
  void Wait();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  bool AtExpectedState();
  Panel* panel_;
  bool expect_active_;
  bool seen_;  // true after transition to expected state has been seen
  bool running_;  // indicates whether message loop is running
  content::NotificationRegistrar registrar_;
  scoped_refptr<ui_test_utils::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(PanelActiveStateObserver);
};

#endif  // CHROME_BROWSER_UI_PANELS_TEST_PANEL_ACTIVE_STATE_OBSERVER_H_
