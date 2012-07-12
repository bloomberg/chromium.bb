// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/constrained_window_frame_simple.h"

#include "chrome/browser/ui/views/constrained_window_views.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/widget.h"

ConstrainedWindowFrameSimple::ConstrainedWindowFrameSimple(
    ConstrainedWindowViews* container) {
  // Draw a custom frame, ie NO frame.
  container->set_frame_type(views::Widget::FRAME_TYPE_FORCE_CUSTOM);
}

gfx::Rect ConstrainedWindowFrameSimple::GetBoundsForClientView() const {
  return bounds();
}

gfx::Rect ConstrainedWindowFrameSimple::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return client_bounds;
}

int ConstrainedWindowFrameSimple::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;
  return HTCLIENT;
}

