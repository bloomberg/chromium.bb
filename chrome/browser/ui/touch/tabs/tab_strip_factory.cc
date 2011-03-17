// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is only compiled when touchui is defined.

#include "chrome/browser/ui/views/tabs/tab_strip_factory.h"

#include "chrome/browser/ui/touch/tabs/touch_tab_strip.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"

// The implmentation of CreateTabStrip for touchui creates a TouchTabStrip
AbstractTabStripView* CreateTabStrip(Browser* browser,
                                     views::View* parent,
                                     TabStripModel* model,
                                     bool use_vertical_tabs) {
  BrowserTabStripController* tabstrip_controller =
      new BrowserTabStripController(browser, model);
  // Ownership of this controller is given to a specific tabstrip when we
  // construct it below.

  TouchTabStrip* tabstrip = new TouchTabStrip(tabstrip_controller);
  parent->AddChildView(tabstrip);
  tabstrip_controller->InitFromModel(tabstrip);
  return tabstrip;
}

