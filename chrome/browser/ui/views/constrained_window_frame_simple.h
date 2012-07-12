// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_FRAME_SIMPLE_H_
#define CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_FRAME_SIMPLE_H_

#include "ui/views/window/non_client_view.h"

class ConstrainedWindowViews;

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowFrameView
// Provides a custom Window frame for ConstrainedWindowViews.
// Implements a simple window with a minimal frame.
class ConstrainedWindowFrameSimple : public views::NonClientFrameView {
 public:
  explicit ConstrainedWindowFrameSimple(ConstrainedWindowViews* container);
  virtual ~ConstrainedWindowFrameSimple() {}

 private:
  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE {}
  virtual void ResetWindowControls() OVERRIDE {}
  virtual void UpdateWindowIcon() OVERRIDE {}

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowFrameSimple);
};
#endif  // CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_FRAME_SIMPLE_H_
