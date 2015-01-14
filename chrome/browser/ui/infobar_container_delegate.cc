// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/infobar_container_delegate.h"
#include "ui/gfx/animation/slide_animation.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/window/non_client_view.h"
#endif

// static
#if defined(OS_MACOSX)
const int InfoBarContainerDelegate::kSeparatorLineHeight = 1;
const int InfoBarContainerDelegate::kDefaultArrowTargetHeight = 11;
#elif defined(TOOLKIT_VIEWS)
// Views comes second until the Mac browser is Views-based.
const int InfoBarContainerDelegate::kSeparatorLineHeight =
    views::NonClientFrameView::kClientEdgeThickness;
const int InfoBarContainerDelegate::kDefaultArrowTargetHeight = 9;
#endif

const int InfoBarContainerDelegate::kDefaultBarTargetHeight = 36;
const int InfoBarContainerDelegate::kMaximumArrowTargetHeight = 24;
const int InfoBarContainerDelegate::kDefaultArrowTargetHalfWidth =
    kDefaultArrowTargetHeight;
const int InfoBarContainerDelegate::kMaximumArrowTargetHalfWidth = 14;

InfoBarContainerDelegate::InfoBarContainerDelegate()
    : top_arrow_target_height_(kDefaultArrowTargetHeight) {
}

InfoBarContainerDelegate::~InfoBarContainerDelegate() {
}

void InfoBarContainerDelegate::SetMaxTopArrowHeight(
    int height,
    infobars::InfoBarContainer* container) {
  // Decrease the height by the arrow stroke thickness, which is the separator
  // line height, because the infobar arrow target heights are without-stroke.
  top_arrow_target_height_ = std::min(
      std::max(height - kSeparatorLineHeight, 0), kMaximumArrowTargetHeight);
  container->UpdateInfoBarArrowTargetHeights();
}

int InfoBarContainerDelegate::ArrowTargetHeightForInfoBar(
    size_t index,
    const gfx::SlideAnimation& animation) const {
  if (!DrawInfoBarArrows(nullptr))
    return 0;
  if (index == 0)
    return top_arrow_target_height_;
  if ((index > 1) || animation.IsShowing())
    return kDefaultArrowTargetHeight;
  // When the first infobar is animating closed, we animate the second infobar's
  // arrow target height from the default to the top target height.  Note that
  // the animation values here are going from 1.0 -> 0.0 as the top bar closes.
  return top_arrow_target_height_ + static_cast<int>(
      (kDefaultArrowTargetHeight - top_arrow_target_height_) *
          animation.GetCurrentValue());
}

void InfoBarContainerDelegate::ComputeInfoBarElementSizes(
    const gfx::SlideAnimation& animation,
    int arrow_target_height,
    int bar_target_height,
    int* arrow_height,
    int* arrow_half_width,
    int* bar_height) const {
  // Find the desired arrow height/half-width.  The arrow area is
  // *arrow_height * *arrow_half_width.  When the bar is opening or closing,
  // scaling each of these with the square root of the animation value causes a
  // linear animation of the area, which matches the perception of the animation
  // of the bar portion.
  double scale_factor = sqrt(animation.GetCurrentValue());
  *arrow_height = static_cast<int>(arrow_target_height * scale_factor);
  if (animation.is_animating()) {
    *arrow_half_width = static_cast<int>(
        std::min(arrow_target_height, kMaximumArrowTargetHalfWidth) *
            scale_factor);
  } else {
    // When the infobar is not animating (i.e. fully open), we set the
    // half-width to be proportionally the same distance between its default and
    // maximum values as the height is between its.
    *arrow_half_width = kDefaultArrowTargetHalfWidth +
        ((kMaximumArrowTargetHalfWidth - kDefaultArrowTargetHalfWidth) *
         ((*arrow_height - kDefaultArrowTargetHeight) /
          (kMaximumArrowTargetHeight - kDefaultArrowTargetHeight)));
  }
  // Add pixels for the stroke, if the arrow is to be visible at all.  Without
  // this, changing the arrow height from 0 to kSeparatorLineHeight would
  // produce no visible effect, because the stroke would paint atop the divider
  // line above the infobar.
  if (*arrow_height)
    *arrow_height += kSeparatorLineHeight;

  *bar_height = animation.CurrentValueBetween(
      0,
      (bar_target_height == -1) ? kDefaultBarTargetHeight : bar_target_height);
}
