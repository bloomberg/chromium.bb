// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_RESOURCES_H_
#define CHROME_BROWSER_UI_TABS_TAB_RESOURCES_H_
#pragma once

namespace gfx {
class Path;
}

// Common resources for tab widgets. Currently this is used on Views and Gtk,
// but not on Cocoa.
class TabResources {
 public:
  // Return a |path| containing the region that matches the bitmap display of
  // a tab of the given |width| and |height|, for input event hit testing.
  static void GetHitTestMask(int width, int height, gfx::Path* path);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_RESOURCES_H_
