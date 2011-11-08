// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "views/window/non_client_view.h"

class AvatarMenuButton;
class BrowserFrame;
class BrowserView;

// A specialization of the NonClientFrameView object that provides additional
// Browser-specific methods.
class BrowserNonClientFrameView : public views::NonClientFrameView {
 public:
  BrowserNonClientFrameView(BrowserFrame* frame, BrowserView* browser_view);
  virtual ~BrowserNonClientFrameView();

  AvatarMenuButton* avatar_button() const { return avatar_button_.get(); }

  // Returns the bounds within which the TabStrip should be laid out.
  virtual gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const = 0;

  // Returns the y coordinate within the window at which the horizontal TabStrip
  // begins, or (in vertical tabs mode) would begin.  If |restored| is true,
  // this is calculated as if we were in restored mode regardless of the current
  // mode.  This is used to correctly align theme images.
  virtual int GetHorizontalTabStripVerticalOffset(bool restored) const = 0;

  // Updates the throbber.
  virtual void UpdateThrobber(bool running) = 0;

 protected:
  BrowserView* browser_view() const { return browser_view_; }
  BrowserFrame* frame() const { return frame_; }

  // Updates the title and icon of the avatar button.
  void UpdateAvatarInfo();

 private:
  // The frame that hosts this view.
  BrowserFrame* frame_;

  // The BrowserView hosted within this View.
  BrowserView* browser_view_;

  // Menu button that displays that either the incognito icon or the profile
  // icon.  May be NULL for some frame styles.
  scoped_ptr<AvatarMenuButton> avatar_button_;
};

namespace browser {

// Provided by a browser_non_client_frame_view_factory_*.cc implementation
BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame, BrowserView* browser_view);

}  // browser

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_H_
