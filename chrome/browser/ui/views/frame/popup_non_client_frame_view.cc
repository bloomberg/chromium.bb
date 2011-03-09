// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/popup_non_client_frame_view.h"

#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

#if defined(OS_LINUX)
#include "views/window/hit_test.h"
#endif

gfx::Rect PopupNonClientFrameView::GetBoundsForClientView() const {
  return gfx::Rect(0, 0, width(), height());
}

bool PopupNonClientFrameView::AlwaysUseCustomFrame() const {
  return false;
}

bool PopupNonClientFrameView::AlwaysUseNativeFrame() const {
  return true;
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

void PopupNonClientFrameView::EnableClose(bool enable) {
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
