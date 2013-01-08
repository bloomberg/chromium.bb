// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/infobars/infobar.h"

#include <cmath>

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/infobars/infobar_container.h"
#include "ui/base/animation/slide_animation.h"

SkColor GetInfoBarTopColor(InfoBarDelegate::Type infobar_type) {
  // Yellow
  static const SkColor kWarningBackgroundColorTop =
      SkColorSetRGB(255, 242, 183);
  // Gray
  static const SkColor kPageActionBackgroundColorTop =
      SkColorSetRGB(237, 237, 237);

  return (infobar_type == InfoBarDelegate::WARNING_TYPE) ?
      kWarningBackgroundColorTop : kPageActionBackgroundColorTop;
}

SkColor GetInfoBarBottomColor(InfoBarDelegate::Type infobar_type) {
  // Yellow
  static const SkColor kWarningBackgroundColorBottom =
      SkColorSetRGB(250, 230, 145);
  // Gray
  static const SkColor kPageActionBackgroundColorBottom =
      SkColorSetRGB(217, 217, 217);

  return (infobar_type == InfoBarDelegate::WARNING_TYPE) ?
      kWarningBackgroundColorBottom : kPageActionBackgroundColorBottom;
}

// TODO(pkasting): Port Mac to use this.
#if defined(TOOLKIT_VIEWS) || defined(TOOLKIT_GTK)

InfoBar::InfoBar(InfoBarService* owner, InfoBarDelegate* delegate)
    : owner_(owner),
      delegate_(delegate),
      container_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_(this)),
      arrow_height_(0),
      arrow_target_height_(kDefaultArrowTargetHeight),
      arrow_half_width_(0),
      bar_height_(0),
      bar_target_height_(kDefaultBarTargetHeight) {
  DCHECK(owner_ != NULL);
  DCHECK(delegate_ != NULL);
  animation_.SetTweenType(ui::Tween::LINEAR);
}

InfoBar::~InfoBar() {
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
  DCHECK_LE(height, kMaximumArrowTargetHeight);
  // Once the closing animation starts, we ignore further requests to change the
  // target height.
  if ((arrow_target_height_ != height) && !animation_.IsClosing()) {
    arrow_target_height_ = height;
    RecalculateHeights(false);
  }
}

void InfoBar::CloseSoon() {
  owner_ = NULL;
  PlatformSpecificOnCloseSoon();
  MaybeDelete();
}

void InfoBar::AnimationProgressed(const ui::Animation* animation) {
  RecalculateHeights(false);
}

void InfoBar::RemoveSelf() {
  // |owner_| should never be NULL here.  If it is, then someone violated what
  // they were supposed to do -- e.g. a ConfirmInfoBarDelegate subclass returned
  // true from Accept() or Cancel() even though the infobar was already closing.
  // In the worst case, if we also switched tabs during that process, then
  // |this| has already been destroyed.  But if that's the case, then we're
  // going to deref a garbage |this| pointer here whether we check |owner_| or
  // not, and in other cases (where we're still closing and |this| is valid),
  // checking |owner_| here will avoid a NULL deref.
  if (owner_)
    owner_->RemoveInfoBar(delegate_);
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
  double scale_factor = sqrt(animation_.GetCurrentValue());
  arrow_height_ = static_cast<int>(arrow_target_height_ * scale_factor);
  if (animation_.is_animating()) {
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

  bar_height_ = animation_.CurrentValueBetween(0, bar_target_height_);

  // Don't re-layout if nothing has changed, e.g. because the animation step was
  // not large enough to actually change the heights by at least a pixel.
  bool heights_differ =
      (old_arrow_height != arrow_height_) || (old_bar_height != bar_height_);
  if (heights_differ)
    PlatformSpecificOnHeightsRecalculated();

  if (container_ && (heights_differ || force_notify))
    container_->OnInfoBarStateChanged(animation_.is_animating());
}

void InfoBar::MaybeDelete() {
  if (!owner_ && delegate_ && (animation_.GetCurrentValue() == 0.0)) {
    if (container_)
      container_->RemoveInfoBar(this);
    delete delegate_;
    delegate_ = NULL;
  }
}

#endif  // TOOLKIT_VIEWS || TOOLKIT_GTK
