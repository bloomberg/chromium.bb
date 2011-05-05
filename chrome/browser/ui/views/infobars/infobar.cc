// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar.h"

#include <cmath>

#include "ui/base/animation/slide_animation.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/infobar_container.h"

InfoBar::InfoBar(InfoBarDelegate* delegate)
    : delegate_(delegate),
      container_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_(new ui::SlideAnimation(this))),
      arrow_height_(0),
      arrow_target_height_(kDefaultArrowTargetHeight),
      arrow_half_width_(0),
      bar_height_(0),
      bar_target_height_(kDefaultBarTargetHeight) {
  DCHECK(delegate != NULL);
  animation_->SetTweenType(ui::Tween::LINEAR);
}

InfoBar::~InfoBar() {
}

void InfoBar::Show(bool animate) {
  if (animate) {
    animation_->Show();
  } else {
    animation_->Reset(1.0);
    AnimationEnded(NULL);
  }
}

void InfoBar::Hide(bool animate) {
  PlatformSpecificHide(animate);
  if (animate) {
    animation_->Hide();
  } else {
    animation_->Reset(0.0);
    AnimationEnded(NULL);
  }
}

void InfoBar::SetArrowTargetHeight(int height) {
  DCHECK_LE(height, kMaximumArrowTargetHeight);
  // Once the closing animation starts, we ignore further requests to change the
  // target height.
  if ((arrow_target_height_ != height) && !animation()->IsClosing()) {
    arrow_target_height_ = height;
    RecalculateHeights(false);
  }
}

void InfoBar::AnimationProgressed(const ui::Animation* animation) {
  RecalculateHeights(false);
}

void InfoBar::RemoveInfoBar() {
  if (container_)
    container_->RemoveDelegate(delegate_);
}

void InfoBar::SetBarTargetHeight(int height) {
  if (bar_target_height_ != height) {
    bar_target_height_ = height;
    RecalculateHeights(false);
  }
}

int InfoBar::OffsetY(const gfx::Size& prefsize) const {
  return arrow_height_ +
      std::max((bar_target_height_ - prefsize.height()) / 2, 0) -
      (bar_target_height_ - bar_height_);
}

void InfoBar::AnimationEnded(const ui::Animation* animation) {
  // When the animation ends, we must ensure the container is notified even if
  // the heights haven't changed, lest it never get an "animation finished"
  // notification.  (If the browser doesn't get this notification, it will not
  // bother to re-layout the content area for the new infobar size.)
  RecalculateHeights(true);
  MaybeDelete();
}

void InfoBar::RecalculateHeights(bool force_notify) {
  int old_arrow_height = arrow_height_;
  int old_bar_height = bar_height_;

  // Find the desired arrow height/half-width.  The arrow area is
  // |arrow_height_| * |arrow_half_width_|.  When the bar is opening or closing,
  // scaling each of these with the square root of the animation value causes a
  // linear animation of the area, which matches the perception of the animation
  // of the bar portion.
  double scale_factor = sqrt(animation()->GetCurrentValue());
  arrow_height_ = static_cast<int>(arrow_target_height_ * scale_factor);
  if (animation_->is_animating()) {
    arrow_half_width_ = static_cast<int>(std::min(arrow_target_height_,
        kMaximumArrowTargetHalfWidth) * scale_factor);
  } else {
    // When the infobar is not animating (i.e. fully open), we set the
    // half-width to be proportionally the same distance between its default and
    // maximum values as the height is between its.
    arrow_half_width_ = kDefaultArrowTargetHalfWidth +
        ((kMaximumArrowTargetHalfWidth - kDefaultArrowTargetHalfWidth) *
         ((arrow_height_ - kDefaultArrowTargetHeight) /
          (kMaximumArrowTargetHeight - kDefaultArrowTargetHeight)));
  }
  // Add pixels for the stroke, if the arrow is to be visible at all.  Without
  // this, changing the arrow height from 0 to kSeparatorLineHeight would
  // produce no visible effect, because the stroke would paint atop the divider
  // line above the infobar.
  if (arrow_height_)
    arrow_height_ += kSeparatorLineHeight;

  bar_height_ =
      static_cast<int>(bar_target_height_ * animation()->GetCurrentValue());

  // Don't re-layout if nothing has changed, e.g. because the animation step was
  // not large enough to actually change the heights by at least a pixel.
  bool heights_differ =
      (old_arrow_height != arrow_height_) || (old_bar_height != bar_height_);
  if (heights_differ)
    PlatformSpecificOnHeightsRecalculated();

  if (container_ && (heights_differ || force_notify))
    container_->OnInfoBarStateChanged(animation_->is_animating());
}

void InfoBar::MaybeDelete() {
  if (delegate_ && (animation_->GetCurrentValue() == 0.0)) {
    if (container_)
      container_->RemoveInfoBar(this);
    // Note that we only tell the delegate we're closed here, and not when we're
    // simply destroyed (by virtue of a tab switch or being moved from window to
    // window), since this action can cause the delegate to destroy itself.
    delegate_->InfoBarClosed();
    delegate_ = NULL;
  }
}
