// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_LAYOUT_TYPE_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_LAYOUT_TYPE_H_

// Defines what should happen when the tabs won't fit at their ideal size.
enum TabStripLayoutType {
  // WARNING: these values are persisted to prefs.

  // Tabs shrink to accomodate the available space. This is the default.
  TAB_STRIP_LAYOUT_SHRINK  = 0,

  // The tabs are always sized to their ideal size and stacked on top of each
  // other so that only a certain set of tabs are visible. This is used when
  // the user uses a touch device (or a command line option is set).
  TAB_STRIP_LAYOUT_STACKED = 1,
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_LAYOUT_TYPE_H_
