// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/frame/touch_browser_frame_view.h"

#include "ui/gfx/compositor/layer.h"
#include "ui/views/desktop/desktop_window_view.h"
#include "views/controls/button/image_button.h"

// static
const char TouchBrowserFrameView::kViewClassName[] =
    "browser/ui/touch/frame/TouchBrowserFrameView";

///////////////////////////////////////////////////////////////////////////////
// TouchBrowserFrameView, public:

TouchBrowserFrameView::TouchBrowserFrameView(BrowserFrame* frame,
                                             BrowserView* browser_view)
    : OpaqueBrowserFrameView(frame, browser_view),
      initialized_screen_rotation_(false) {
}

TouchBrowserFrameView::~TouchBrowserFrameView() {
}

std::string TouchBrowserFrameView::GetClassName() const {
  return kViewClassName;
}

bool TouchBrowserFrameView::HitTest(const gfx::Point& point) const {
  if (OpaqueBrowserFrameView::HitTest(point))
    return true;

  if (close_button()->IsVisible() &&
      close_button()->GetMirroredBounds().Contains(point))
    return true;
  if (restore_button()->IsVisible() &&
      restore_button()->GetMirroredBounds().Contains(point))
    return true;
  if (maximize_button()->IsVisible() &&
      maximize_button()->GetMirroredBounds().Contains(point))
    return true;
  if (minimize_button()->IsVisible() &&
      minimize_button()->GetMirroredBounds().Contains(point))
    return true;

  return false;
}
