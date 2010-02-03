// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_MANAGER_H_
#define CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_MANAGER_H_

#include "base/gfx/size.h"
#include "views/layout_manager.h"

// An extended LayoutManager to layout components in
// BrowserView.
class BrowserViewLayoutManager : public views::LayoutManager {
 public:
  // Returns the minimum size of the browser view.
  virtual gfx::Size GetMinimumSize() = 0;

  // Returns the bounding box for the find bar.
  virtual gfx::Rect GetFindBarBoundingBox() const = 0;

  // Returns true if the specified point(BrowserView coordinates) is in
  // in the window caption area of the browser window.
  virtual bool IsPositionInWindowCaption(const gfx::Point& point) = 0;

  // Tests to see if the specified |point| (in nonclient view's coordinates)
  // is within the views managed by the laymanager. Returns one of
  // HitTestCompat enum defined in views/window/hit_test.h.
  // See also ClientView::NonClientHitTest.
  virtual int NonClientHitTest(const gfx::Point& point) = 0;
};

#endif  // CHROME_BROWSER_VIEWS_FRAME_BROWSER_VIEW_LAYOUT_MANAGER_H_
