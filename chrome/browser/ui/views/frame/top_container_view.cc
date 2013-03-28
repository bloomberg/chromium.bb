// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/top_container_view.h"

#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"

TopContainerView::TopContainerView(BrowserView* browser_view)
    : browser_view_(browser_view) {
}

TopContainerView::~TopContainerView() {
}

gfx::Size TopContainerView::GetPreferredSize() {
  // The view wants to be as wide as its parent and tall enough to fully show
  // its last child view.
  int last_child_bottom =
      child_count() > 0 ? child_at(child_count() - 1)->bounds().bottom() : 0;
  return gfx::Size(browser_view_->width(), last_child_bottom);
}

std::string TopContainerView::GetClassName() const {
  return "TopContainerView";
}

void TopContainerView::PaintChildren(gfx::Canvas* canvas) {
  if (browser_view_->immersive_mode_controller()->IsRevealed()) {
    // Top-views depend on parts of the frame (themes, window buttons) being
    // painted underneath them. Clip rect has already been set to the bounds
    // of this view, so just paint the frame.
    views::View* frame = browser_view_->frame()->GetFrameView();
    frame->Paint(canvas);
  }

  views::View::PaintChildren(canvas);
}
