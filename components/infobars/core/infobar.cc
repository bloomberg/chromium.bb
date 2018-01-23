// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/core/infobar.h"

#include <cmath>
#include <utility>

#include "base/logging.h"
#include "build/build_config.h"
#include "components/infobars/core/infobar_container.h"
#include "components/infobars/core/infobar_manager.h"
#include "ui/gfx/animation/slide_animation.h"

namespace infobars {

InfoBar::InfoBar(std::unique_ptr<InfoBarDelegate> delegate)
    : owner_(nullptr),
      delegate_(std::move(delegate)),
      container_(nullptr),
      animation_(this),
      arrow_height_(0),
      arrow_target_height_(0),
      arrow_half_width_(0),
      bar_height_(0),
      bar_target_height_(-1) {
  DCHECK(delegate_ != nullptr);
  animation_.SetTweenType(gfx::Tween::LINEAR);
  delegate_->set_infobar(this);
}

InfoBar::~InfoBar() {
  DCHECK(!owner_);
}

void InfoBar::SetOwner(InfoBarManager* owner) {
  DCHECK(!owner_);
  owner_ = owner;
  delegate_->set_nav_entry_id(owner->GetActiveEntryID());
  PlatformSpecificSetOwner();
}

void InfoBar::Show(bool animate) {
  PlatformSpecificShow(animate);
  if (animate) {
    animation_.Show();
  } else {
    animation_.Reset(1.0);
    RecalculateHeights(true);
  }
}

void InfoBar::Hide(bool animate) {
  PlatformSpecificHide(animate);
  if (animate) {
    animation_.Hide();
  } else {
    animation_.Reset(0.0);
    // We want to remove ourselves from the container immediately even if we
    // still have an owner, which MaybeDelete() won't do.
    DCHECK(container_);
    container_->RemoveInfoBar(this);
    MaybeDelete();  // Necessary if the infobar was already closing.
  }
}

void InfoBar::SetArrowTargetHeight(int height) {
  // Once the closing animation starts, we ignore further requests to change the
  // target height.
  if ((arrow_target_height_ != height) && !animation_.IsClosing()) {
    arrow_target_height_ = height;
    RecalculateHeights(false);
  }
}

void InfoBar::CloseSoon() {
  owner_ = nullptr;
  PlatformSpecificOnCloseSoon();
  MaybeDelete();
}

void InfoBar::RemoveSelf() {
  if (owner_)
    owner_->RemoveInfoBar(this);
}

void InfoBar::SetBarTargetHeight(int height) {
  if (bar_target_height_ != height) {
    bar_target_height_ = height;
    RecalculateHeights(false);
  }
}

void InfoBar::AnimationProgressed(const gfx::Animation* animation) {
  RecalculateHeights(false);
}

void InfoBar::AnimationEnded(const gfx::Animation* animation) {
  // When the animation ends, we must ensure the container is notified even if
  // the heights haven't changed, lest it never get an "animation finished"
  // notification.  (If the browser doesn't get this notification, it will not
  // bother to re-layout the content area for the new infobar size.)
  RecalculateHeights(true);
  MaybeDelete();
}

void InfoBar::RecalculateHeights(bool force_notify) {
  // If there's no container delegate, there's no way to compute new element
  // sizes, so return immediately.  We don't need to worry that this might leave
  // us with bogus sizes, because if we're ever re-added to a container, it will
  // call Show(false) while re-adding us, which will compute a correct set of
  // sizes.
  if (!container_ || !container_->delegate())
    return;

  int old_arrow_height = arrow_height_;
  int old_bar_height = bar_height_;

  container_->delegate()->ComputeInfoBarElementSizes(
      animation_, arrow_target_height_, bar_target_height_, &arrow_height_,
      &arrow_half_width_, &bar_height_);

  // Don't re-layout if nothing has changed, e.g. because the animation step was
  // not large enough to actually change the heights by at least a pixel.
  bool heights_differ =
      (old_arrow_height != arrow_height_) || (old_bar_height != bar_height_);
  if (heights_differ)
    PlatformSpecificOnHeightsRecalculated();

  if (heights_differ || force_notify)
    container_->OnInfoBarStateChanged(animation_.is_animating());
}

void InfoBar::MaybeDelete() {
  if (!owner_ && (animation_.GetCurrentValue() == 0.0)) {
    if (container_)
      container_->RemoveInfoBar(this);
    delete this;
  }
}

}  // namespace infobars
