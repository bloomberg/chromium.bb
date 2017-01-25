// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/view_stack.h"

#include "base/memory/ptr_util.h"
#include "ui/views/layout/fill_layout.h"

ViewStack::ViewStack()
    : slide_in_animator_(base::MakeUnique<views::BoundsAnimator>(this)),
      slide_out_animator_(base::MakeUnique<views::BoundsAnimator>(this)) {
  SetLayoutManager(new views::FillLayout());

  slide_out_animator_->AddObserver(this);
  // Paint to a layer and Mask to Bounds, otherwise descendant views that paint
  // to a layer themselves will still paint while they're being animated out and
  // are out of bounds of their parent.
  SetPaintToLayer();
  layer()->SetMasksToBounds(true);
}

ViewStack::~ViewStack() {}

void ViewStack::Push(std::unique_ptr<views::View> view, bool animate) {
  gfx::Rect destination = bounds();
  destination.set_origin(gfx::Point(0, 0));

  if (animate) {
    // First add the new view out of bounds since it'll slide in from right to
    // left.
    view->SetBounds(width(), 0, width(), height());
    view->Layout();

    AddChildView(view.get());

    // Animate the new view to be right on top of this one.
    slide_in_animator_->AnimateViewTo(view.get(), destination);
  } else {
    view->SetBoundsRect(destination);
    view->Layout();
    AddChildView(view.get());
  }

  view->set_owned_by_client();
  // Add the new view to the stack so it can be popped later when navigating
  // back to the previous screen.
  stack_.push_back(std::move(view));
}

void ViewStack::Pop() {
  gfx::Rect destination = bounds();
  destination.set_origin(gfx::Point(width(), 0));

  slide_out_animator_->AnimateViewTo(
      stack_.back().get(), destination);
}

bool ViewStack::CanProcessEventsWithinSubtree() const {
  return !slide_in_animator_->IsAnimating() &&
         !slide_out_animator_->IsAnimating();
}

void ViewStack::Layout() {
  views::View::Layout();

  // If this view's bounds changed since the beginning of an animation, the
  // animator's targets have to be changed as well.
  gfx::Rect in_new_destination = bounds();
  in_new_destination.set_origin(gfx::Point(0, 0));
  UpdateAnimatorBounds(slide_in_animator_.get(), in_new_destination);


  gfx::Rect out_new_destination = bounds();
  out_new_destination.set_origin(gfx::Point(width(), 0));
  UpdateAnimatorBounds(slide_out_animator_.get(), out_new_destination);
}

void ViewStack::UpdateAnimatorBounds(
    views::BoundsAnimator* animator, const gfx::Rect& target) {
  // If an animator is currently animating, figure out which views and update
  // their target bounds.
  if (animator->IsAnimating()) {
    for (auto& view: stack_) {
      if (animator->IsAnimating(view.get())) {
        animator->SetTargetBounds(view.get(), target);
      }
    }
  }
}

void ViewStack::OnBoundsAnimatorDone(views::BoundsAnimator* animator) {
  // This should only be called from slide_out_animator_ when the views going
  // out are done animating.
  DCHECK_EQ(animator, slide_out_animator_.get());

  stack_.pop_back();
  DCHECK(!stack_.empty()) << "State stack should never be empty";
}
