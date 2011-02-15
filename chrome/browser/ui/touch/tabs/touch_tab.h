// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOUCH_TABS_TOUCH_TAB_H_
#define CHROME_BROWSER_UI_TOUCH_TABS_TOUCH_TAB_H_
#pragma once

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/browser/ui/views/tabs/base_tab.h"
#include "ui/gfx/point.h"


///////////////////////////////////////////////////////////////////////////////
//
// TouchTab
//
//  A View that renders a TouchTab in a TouchTabStrip
//
///////////////////////////////////////////////////////////////////////////////
class TouchTab : public BaseTab {
 public:
  // The menu button's class name.
  static const char kViewClassName[];

  explicit TouchTab(TabController* controller);
  virtual ~TouchTab();

  // Set the background offset used to match the image in the inactive tab
  // to the frame image.
  void set_background_offset(const gfx::Point& offset) {
    background_offset_ = offset;
  }

  // Returns the minimum possible size of a single unselected Tab.
  static gfx::Size GetMinimumUnselectedSize();

 protected:
  virtual const gfx::Rect& GetTitleBounds() const;
  virtual const gfx::Rect& GetIconBounds() const;

 private:
  // Overridden from views::View:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual bool HasHitTestMask() const;
  virtual void GetHitTestMask(gfx::Path* path) const;

  // Paint various portions of the Tab
  void PaintTabBackground(gfx::Canvas* canvas);
  void PaintIcon(gfx::Canvas* canvas);
  void PaintActiveTabBackground(gfx::Canvas* canvas);

  // TODO(wyck): will eventually add OnTouchEvent when the Touch Tab Strip
  // requires touch-specific event handling.

  // Performs a one-time initialization of static resources such as tab images.
  static void InitTabResources();

  // Loads the images to be used for the tab background.
  static void LoadTabImages();

  // the bounds of the title text
  gfx::Rect title_bounds_;

  // the bounds of the favicon
  gfx::Rect favicon_bounds_;

  // The offset used to paint the inactive background image.
  gfx::Point background_offset_;

  // 'l' is for left
  // 'c' is for center
  // 'r' is for right
  struct TouchTabImage {
    SkBitmap* image_l;
    SkBitmap* image_c;
    SkBitmap* image_r;
    int l_width;
    int r_width;
    int y_offset;
  };
  static TouchTabImage tab_active;
  static TouchTabImage tab_inactive;
  static TouchTabImage tab_alpha;

  DISALLOW_COPY_AND_ASSIGN(TouchTab);
};

#endif  // CHROME_BROWSER_UI_TOUCH_TABS_TOUCH_TAB_H_
