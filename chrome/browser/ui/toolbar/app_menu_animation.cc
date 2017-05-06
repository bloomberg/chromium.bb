// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/app_menu_animation.h"

#include "base/memory/ptr_util.h"
#include "cc/paint/paint_flags.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/skia_util.h"

namespace {

// Duration of the open and close animations in ms.
constexpr float kOpenDurationMs = 733.0f;
constexpr float kCloseDurationMs = 283.0f;

// Duration of the color animation in ms.
constexpr float kColorDurationMs = 100.0f;

// The radius of each dot in the icon.
constexpr float kDotRadius = 2.0f;

// The % the top and bottom dots need to be offset from the middle.
constexpr float kDotYOffset = 0.32f;

// Value of the stroke when the icon is opened or closed.
constexpr float kCloseStroke = 0.204f;
constexpr float kOpenStroke = 0.136f;

// Value of the width when the animation is fully opened.
constexpr float kOpenWidth = 0.52f;

// The delay of the color and dot animations in ms.
constexpr float kColorDelayMs = 33.33f;
constexpr float kDotDelayMs = 66.67f;

// The % of time it takes for each dot to animate to its full width.
constexpr float kTopWidthOpenInterval = 533.3f / kOpenDurationMs;
constexpr float kMiddleWidthOpenInterval = 383.3f / kOpenDurationMs;
constexpr float kBottomWidthOpenInterval = 400.0f / kOpenDurationMs;

// The % of time it takes for each dot to animate to its final stroke.
constexpr float kTopStrokeOpenInterval = 400.0f / kOpenDurationMs;
constexpr float kMiddleStrokeOpenInterval = 283.3f / kOpenDurationMs;
constexpr float kBottomStrokeOpenInterval = 266.7f / kOpenDurationMs;

// The % of time it takes for each dot to animate its width and stroke.
constexpr float kWidthStrokeCloseInterval = 150.0f / kCloseDurationMs;

}  // namespace

AppMenuAnimation::AppMenuDot::AppMenuDot(base::TimeDelta delay,
                                         float width_open_interval,
                                         float stroke_open_interval)
    : delay_(delay),
      width_open_interval_(width_open_interval),
      stroke_open_interval_(stroke_open_interval) {}

void AppMenuAnimation::AppMenuDot::Paint(const gfx::PointF& center_point,
                                         SkColor start_color,
                                         SkColor target_color,
                                         gfx::Canvas* canvas,
                                         const gfx::Rect& bounds,
                                         const gfx::SlideAnimation* animation,
                                         AppMenuAnimationDelegate* delegate) {
  bool is_opening = animation->IsShowing();
  float total_duration = is_opening ? kOpenDurationMs : kCloseDurationMs;
  float width_duration =
      is_opening ? width_open_interval_ : kWidthStrokeCloseInterval;
  float stroke_duration =
      is_opening ? stroke_open_interval_ : kWidthStrokeCloseInterval;

  // When the animation is closing, each dot uses the remainder of the full
  // delay period (2 * kDotDelayMs). The results should be (0->2x, 1x->1x,
  // 2x->0).
  base::TimeDelta delay =
      is_opening ? delay_
                 : base::TimeDelta::FromMilliseconds(kDotDelayMs * 2) - delay_;
  float progress =
      animation->GetCurrentValue() - (delay.InMillisecondsF() / total_duration);

  float width_progress = 0.0;
  float stroke_progress = 0.0;
  float color_progress = 0.0;

  if (progress > 0) {
    width_progress = std::min(1.0f, progress / width_duration);
    stroke_progress = std::min(1.0f, progress / stroke_duration);

    if (is_opening) {
      float color_delay_interval = kColorDelayMs / total_duration;
      float color_duration_interval = kColorDurationMs / total_duration;
      if (progress > color_delay_interval) {
        color_progress = std::min(
            1.0f, (progress - color_delay_interval) / color_duration_interval);
      }
    }
  }

  float dot_height =
      gfx::Tween::FloatValueBetween(stroke_progress, kCloseStroke, kOpenStroke);
  dot_height *= bounds.height();

  float dot_width =
      gfx::Tween::FloatValueBetween(width_progress, kCloseStroke, kOpenWidth);
  dot_width *= bounds.width();

  gfx::PointF point = center_point;
  point.Offset(-dot_width / 2, -dot_height / 2);

  SkColor color = is_opening ? gfx::Tween::ColorValueBetween(
                                   color_progress, start_color, target_color)
                             : target_color;

  cc::PaintFlags flags;
  flags.setColor(color);
  flags.setStrokeWidth(dot_height);
  flags.setStrokeCap(cc::PaintFlags::kRound_Cap);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  canvas->DrawRoundRect(gfx::RectF(point, gfx::SizeF(dot_width, dot_height)),
                        kDotRadius, flags);
}

AppMenuAnimation::AppMenuAnimation(AppMenuAnimationDelegate* delegate,
                                   SkColor initial_color)
    : delegate_(delegate),
      animation_(base::MakeUnique<gfx::SlideAnimation>(this)),
      bottom_dot_(base::TimeDelta(),
                  kBottomWidthOpenInterval,
                  kBottomStrokeOpenInterval),
      middle_dot_(base::TimeDelta::FromMilliseconds(kDotDelayMs),
                  kMiddleWidthOpenInterval,
                  kMiddleStrokeOpenInterval),
      top_dot_(base::TimeDelta::FromMilliseconds(kDotDelayMs * 2),
               kTopWidthOpenInterval,
               kTopStrokeOpenInterval),
      start_color_(initial_color),
      target_color_(initial_color) {
  animation_->SetSlideDuration(kOpenDurationMs);
  animation_->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
}

AppMenuAnimation::~AppMenuAnimation() {}

void AppMenuAnimation::PaintAppMenu(gfx::Canvas* canvas,
                                    const gfx::Rect& bounds) {
  gfx::PointF middle_point = gfx::PointF(bounds.CenterPoint());
  float y_offset = kDotYOffset * bounds.height();
  gfx::PointF top_point = middle_point;
  top_point.Offset(0, -y_offset);

  gfx::PointF bottom_point = middle_point;
  bottom_point.Offset(0, y_offset);

  middle_dot_.Paint(middle_point, start_color_, target_color_, canvas, bounds,
                    animation_.get(), delegate_);
  top_dot_.Paint(top_point, start_color_, target_color_, canvas, bounds,
                 animation_.get(), delegate_);
  bottom_dot_.Paint(bottom_point, start_color_, target_color_, canvas, bounds,
                    animation_.get(), delegate_);
}

void AppMenuAnimation::StartAnimation() {
  if (!animation_->is_animating()) {
    animation_->SetSlideDuration(kOpenDurationMs);
    animation_->Show();
    delegate_->AppMenuAnimationStarted();
  }
}

void AppMenuAnimation::AnimationEnded(const gfx::Animation* animation) {
  if (animation_->IsShowing()) {
    animation_->SetSlideDuration(kCloseDurationMs);
    animation_->Hide();
  } else {
    start_color_ = target_color_;
  }

  delegate_->AppMenuAnimationEnded();
}

void AppMenuAnimation::AnimationProgressed(const gfx::Animation* animation) {
  delegate_->InvalidateIcon();
}
