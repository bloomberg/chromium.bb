// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/popup_non_client_frame_view.h"

#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

PopupNonClientFrameView::PopupNonClientFrameView(BrowserFrame* frame)
    : BrowserNonClientFrameView(frame, NULL) {
  frame->set_frame_type(views::Widget::FRAME_TYPE_FORCE_NATIVE);
}

gfx::Rect PopupNonClientFrameView::GetBoundsForClientView() const {
  return gfx::Rect(0, 0, width(), height());
}

gfx::Rect PopupNonClientFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  return client_bounds;
}

int PopupNonClientFrameView::NonClientHitTest(const gfx::Point& point) {
  return bounds().Contains(point) ? HTCLIENT : HTNOWHERE;
}

void PopupNonClientFrameView::GetWindowMask(const gfx::Size& size,
                                                    gfx::Path* window_mask) {
}

void PopupNonClientFrameView::ResetWindowControls() {
}

void PopupNonClientFrameView::UpdateWindowIcon() {
}

gfx::Rect PopupNonClientFrameView::GetBoundsForTabStrip(
    views::View* tabstrip) const {
  return gfx::Rect(0, 0, width(), tabstrip->GetPreferredSize().height());
}

int PopupNonClientFrameView::GetHorizontalTabStripVerticalOffset(
    bool restored) const {
  return 0;
}

void PopupNonClientFrameView::UpdateThrobber(bool running) {
}
