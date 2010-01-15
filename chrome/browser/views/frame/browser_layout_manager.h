// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_LAYOUT_MANAGER_H_
#define CHROME_BROWSER_VIEWS_FRAME_BROWSER_LAYOUT_MANAGER_H_

#include "base/gfx/size.h"
#include "views/layout_manager.h"

class BrowserView;

// An extended LayoutManager to layout components in
// BrowserView.
class BrowserLayoutManager : public views::LayoutManager {
 public:
  // Returns the minimum size of the browser view.
  virtual gfx::Size GetMinimumSize() = 0;

  // Returns the bounding box for the find bar.
  virtual gfx::Rect GetFindBarBoundingBox() const = 0;

  static BrowserLayoutManager* CreateBrowserLayoutManager();
};

#endif  // CHROME_BROWSER_VIEWS_FRAME_BROWSER_LAYOUT_MANAGER_H_

