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
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_(new ui::SlideAnimation(this))) {
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
  if (container_)
    container_->OnInfoBarAnimated(false);
}

void InfoBar::RemoveInfoBar() {
  if (container_)
    container_->RemoveDelegate(delegate_);
}

void InfoBar::PlatformSpecificHide(bool animate) {
}

void InfoBar::AnimationEnded(const ui::Animation* animation) {
  if (container_)
    container_->OnInfoBarAnimated(true);
  MaybeDelete();
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
