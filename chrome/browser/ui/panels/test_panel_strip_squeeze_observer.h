// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_TEST_PANEL_STRIP_SQUEEZE_OBSERVER_H_
#define CHROME_BROWSER_UI_PANELS_TEST_PANEL_STRIP_SQUEEZE_OBSERVER_H_

#include "chrome/browser/ui/panels/test_panel_notification_observer.h"

class DockedPanelStrip;
class Panel;

// Custom notification observer for waiting on panel strip that squeezes
// its panels to reflect a certain state.
// Modeled after ui_test_utils notification observers.
class PanelStripSqueezeObserver : public TestPanelNotificationObserver {
 public:
  // Register to listen for panel strip updated notifications
  // from the specified strip to detect a change to the state
  // where the |active_panel| is at full width and all other
  // panels in the strip are squeezed.
  PanelStripSqueezeObserver(DockedPanelStrip* strip, Panel* active_panel);
  virtual ~PanelStripSqueezeObserver();

 private:
  // TestNotificationObserver override:
  virtual bool AtExpectedState() OVERRIDE;

  bool IsSqueezed(Panel* panel);

  DockedPanelStrip* panel_strip_;
  Panel* active_panel_;

  DISALLOW_COPY_AND_ASSIGN(PanelStripSqueezeObserver);
};

#endif  // CHROME_BROWSER_UI_PANELS_TEST_PANEL_STRIP_SQUEEZE_OBSERVER_H_
