// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_factory.h"

#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/side_tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"

// This default implementation of CreateTabStrip creates a TabStrip or a
// SideTabStrip, depending on whether we are using vertical tabs.
AbstractTabStripView* CreateTabStrip(Browser* browser,
                                     TabStripModel* model,
                                     bool use_vertical_tabs) {
  BrowserTabStripController* tabstrip_controller =
      new BrowserTabStripController(browser, model);
  // Ownership of this controller is given to a specific tabstrip when we
  // construct it below.

  BaseTabStrip* tabstrip = NULL;

  if (use_vertical_tabs)
    tabstrip = new SideTabStrip(tabstrip_controller);
  else
    tabstrip = new TabStrip(tabstrip_controller);
  tabstrip_controller->InitFromModel(tabstrip);
  return tabstrip;
}

