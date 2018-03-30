// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/infobar_container_delegate_ios.h"

#include "base/logging.h"
#include "ios/chrome/browser/infobars/infobar_container_state_delegate.h"
#include "ui/gfx/animation/slide_animation.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

int InfoBarContainerDelegateIOS::ArrowTargetHeightForInfoBar(
    size_t index,
    const gfx::SlideAnimation& animation) const {
  return 0;
}

void InfoBarContainerDelegateIOS::ComputeInfoBarElementSizes(
    const gfx::SlideAnimation& animation,
    int arrow_target_height,
    int bar_target_height,
    int* arrow_height,
    int* arrow_half_width,
    int* bar_height) const {
  DCHECK_NE(-1, bar_target_height)
      << "Infobars don't have a default height on iOS";
  *arrow_height = 0;
  *arrow_half_width = 0;
  *bar_height = animation.CurrentValueBetween(0, bar_target_height);
}

void InfoBarContainerDelegateIOS::InfoBarContainerStateChanged(
    bool is_animating) {
  [delegate_ infoBarContainerStateDidChangeAnimated:is_animating];
}

bool InfoBarContainerDelegateIOS::DrawInfoBarArrows(int* x) const {
  return false;
}
