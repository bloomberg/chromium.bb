// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_factory.h"

#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/side_tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"

// This default implementation of CreateTabStrip creates a TabStrip or a
// SideTabStrip, depending on whether we are using vertical tabs.
BaseTabStrip* CreateTabStrip(BrowserTabStripController* tabstrip_controller,
                             bool use_vertical_tabs) {
  if (use_vertical_tabs)
    return new SideTabStrip(tabstrip_controller);
  else
    return new TabStrip(tabstrip_controller);
}

