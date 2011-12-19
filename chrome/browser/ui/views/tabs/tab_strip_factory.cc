// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_factory.h"

#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"

// This default implementation of CreateTabStrip creates a TabStrip.
AbstractTabStripView* CreateTabStrip(Browser* browser,
                                     views::View* parent,
                                     TabStripModel* model) {
  BrowserTabStripController* tabstrip_controller =
      new BrowserTabStripController(browser, model);
  // Ownership of this controller is given to a specific tabstrip when we
  // construct it below.

  TabStrip* tabstrip = new TabStrip(tabstrip_controller);
  parent->AddChildView(tabstrip);
  tabstrip_controller->InitFromModel(tabstrip);
  return tabstrip;
}

