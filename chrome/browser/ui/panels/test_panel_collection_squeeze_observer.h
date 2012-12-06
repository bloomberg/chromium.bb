// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_TEST_PANEL_COLLECTION_SQUEEZE_OBSERVER_H_
#define CHROME_BROWSER_UI_PANELS_TEST_PANEL_COLLECTION_SQUEEZE_OBSERVER_H_

#include "chrome/browser/ui/panels/test_panel_notification_observer.h"

class DockedPanelCollection;
class Panel;

// Custom notification observer for waiting on panel collection that squeezes
// its panels to reflect a certain state.
// Modeled after ui_test_utils notification observers.
class PanelCollectionSqueezeObserver : public TestPanelNotificationObserver {
 public:
  // Register to listen for panel collection updated notifications
  // from the specified collection to detect a change to the state
  // where the |active_panel| is at full width and all other
  // panels in the collection are squeezed.
  PanelCollectionSqueezeObserver(DockedPanelCollection* collection,
                                 Panel* active_panel);
  virtual ~PanelCollectionSqueezeObserver();

 private:
  // TestNotificationObserver override:
  virtual bool AtExpectedState() OVERRIDE;

  bool IsSqueezed(Panel* panel);

  DockedPanelCollection* panel_collection_;
  Panel* active_panel_;

  DISALLOW_COPY_AND_ASSIGN(PanelCollectionSqueezeObserver);
};

#endif  // CHROME_BROWSER_UI_PANELS_TEST_PANEL_COLLECTION_SQUEEZE_OBSERVER_H_
