// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/infobar.h"

#include "ui/base/animation/slide_animation.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/infobar_container.h"

InfoBar::InfoBar(InfoBarDelegate* delegate)
    : delegate_(delegate),
      container_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_(new ui::SlideAnimation(this))),
      bar_target_height_(kDefaultBarTargetHeight),
      tab_height_(0),
      bar_height_(0) {
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

void InfoBar::AnimationProgressed(const ui::Animation* animation) {
  RecalculateHeight();
}

void InfoBar::RemoveInfoBar() {
  if (container_)
    container_->RemoveDelegate(delegate_);
}

void InfoBar::SetBarTargetHeight(int height) {
  if (bar_target_height_ != height) {
    bar_target_height_ = height;
    RecalculateHeight();
  }
}

int InfoBar::OffsetY(const gfx::Size& prefsize) const {
  return tab_height_ +
      std::max((bar_target_height_ - prefsize.height()) / 2, 0) -
      (bar_target_height_ - bar_height_);
}

void InfoBar::AnimationEnded(const ui::Animation* animation) {
  RecalculateHeight();
  MaybeDelete();
}

void InfoBar::RecalculateHeight() {
  int old_tab_height = tab_height_;
  int old_bar_height = bar_height_;
  tab_height_ =
      static_cast<int>(kTabTargetHeight * animation()->GetCurrentValue());
  bar_height_ =
      static_cast<int>(bar_target_height_ * animation()->GetCurrentValue());

  // Don't re-layout if nothing has changed, e.g. because the animation step was
  // not large enough to actually change the heights by at least a pixel.
  if ((old_tab_height != tab_height_) || (old_bar_height != bar_height_)) {
    PlatformSpecificOnHeightRecalculated();
    if (container_)
      container_->OnInfoBarHeightChanged(animation_->is_animating());
  }
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
