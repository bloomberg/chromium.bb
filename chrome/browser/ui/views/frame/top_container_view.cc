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

const char* TopContainerView::GetClassName() const {
  return "TopContainerView";
}

void TopContainerView::OnPaintBackground(gfx::Canvas* canvas) {
  if (browser_view_->immersive_mode_controller()->IsRevealed()) {
    // Top-views depend on parts of the frame (themes, window title,
    // window controls) being painted underneath them. Clip rect has already
    // been set to the bounds of this view, so just paint the frame.
    views::View* frame = browser_view_->frame()->GetFrameView();
    frame->Paint(canvas, views::CullSet());
  }
}
