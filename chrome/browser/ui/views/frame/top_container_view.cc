// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/top_container_view.h"

#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"

TopContainerView::TopContainerView(BrowserView* browser_view)
    : browser_view_(browser_view),
      focus_manager_(browser_view->GetFocusManager()) {
  focus_manager_->AddFocusChangeListener(this);
}

TopContainerView::~TopContainerView() {
  focus_manager_->RemoveFocusChangeListener(this);
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

void TopContainerView::OnWillChangeFocus(View* focused_before,
                                        View* focused_now) {
}

void TopContainerView::OnDidChangeFocus(View* focused_before,
                                        View* focused_now) {
  // If one of this view's children had focus before, but doesn't have focus
  // now, we may want to slide out the top views in immersive fullscreen.
  if (browser_view_->immersive_mode_controller()->enabled() &&
      Contains(focused_before) &&
      !Contains(focused_now))
    browser_view_->immersive_mode_controller()->OnRevealViewLostFocus();
}
