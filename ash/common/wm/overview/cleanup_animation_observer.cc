// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/overview/cleanup_animation_observer.h"

#include "ui/views/widget/widget.h"

namespace ash {

CleanupAnimationObserver::CleanupAnimationObserver(
    std::unique_ptr<views::Widget> widget)
    : widget_(std::move(widget)), owner_(nullptr) {
  DCHECK(widget_);
}

CleanupAnimationObserver::~CleanupAnimationObserver() {}

void CleanupAnimationObserver::OnImplicitAnimationsCompleted() {
  // |widget_| may get reset if Shutdown() is called prior to this method.
  if (!widget_)
    return;
  if (owner_) {
    owner_->RemoveAndDestroyAnimationObserver(this);
    return;
  }
  delete this;
}

void CleanupAnimationObserver::SetOwner(WindowSelectorDelegate* owner) {
  owner_ = owner;
}

void CleanupAnimationObserver::Shutdown() {
  widget_.reset();
  owner_ = nullptr;
}

}  // namespace ash
