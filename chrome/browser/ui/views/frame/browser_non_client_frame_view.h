// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
#pragma once

#include "views/window/non_client_view.h"

class BrowserFrame;
class BrowserView;

// A specialization of the NonClientFrameView object that provides additional
// Browser-specific methods.
class BrowserNonClientFrameView : public views::NonClientFrameView {
 public:
  BrowserNonClientFrameView() : NonClientFrameView() {}
  virtual ~BrowserNonClientFrameView() {}

  // Returns the bounds within which the TabStrip should be laid out.
  virtual gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const = 0;

  // Returns the y coordinate within the window at which the horizontal TabStrip
  // begins, or (in vertical tabs mode) would begin.  If |restored| is true,
  // this is calculated as if we were in restored mode regardless of the current
  // mode.  This is used to correctly align theme images.
  virtual int GetHorizontalTabStripVerticalOffset(bool restored) const = 0;

  // Updates the throbber.
  virtual void UpdateThrobber(bool running) = 0;
};

namespace browser {

// Provided by a browser_non_client_frame_view_factory_*.cc implementation
BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame, BrowserView* browser_view);

}  // browser

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
