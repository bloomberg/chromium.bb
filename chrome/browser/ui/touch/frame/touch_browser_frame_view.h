// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOUCH_FRAME_TOUCH_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_TOUCH_FRAME_TOUCH_BROWSER_FRAME_VIEW_H_
#pragma once

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"

class BrowserFrame;
class BrowserView;

class TouchBrowserFrameView
    : public OpaqueBrowserFrameView {
 public:
  // Internal class name.
  static const char kViewClassName[];

  // Constructs a non-client view for an BrowserFrame.
  TouchBrowserFrameView(BrowserFrame* frame, BrowserView* browser_view);
  virtual ~TouchBrowserFrameView();

 private:
  // Overridden from views::View
  virtual std::string GetClassName() const OVERRIDE;
  virtual bool HitTest(const gfx::Point& point) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TouchBrowserFrameView);
};

#endif  // CHROME_BROWSER_UI_TOUCH_FRAME_TOUCH_BROWSER_FRAME_VIEW_H_
