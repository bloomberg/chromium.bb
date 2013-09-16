// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/caption_buttons/alternate_frame_caption_button.h"

#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"

namespace ash {

namespace {

// The width and height of the region of the button which does not overlap
// with other buttons.
const int kSize = 32;

// A bubble is painted in the background when the mouse button is pressed.
// When the button is pressed:
//  - The bubble is faded in from opacity 0 to |kShownBubbleOpacity|.
//  - The bubble is expanded from |kInitialGrowBubbleRadius| to
//    |kFullyGrownBubbleRadius|.
// When the button is unpressed (STATE_NORMAL)
//  - The bubble is faded out from its current opacity back to 0.
//  - The bubble is further expanded from its current radius to
//    |kFinalBurstBubbleRadius|.
const int kInitialGrowBubbleRadius = 16;
const int kFullyGrownBubbleRadius = 22;
const int kFinalBurstBubbleRadius = 26;
const int kShownBubbleOpacity = 100;

// The duraton of the animations for hiding and showing the bubble.
const int kBubbleAnimationDuration = 100;

// TODO(pkotwicz): Replace these colors with colors from UX.
const SkColor kPathColor = SK_ColorDKGRAY;
const SkColor kPressedHoveredPathColor = SK_ColorBLACK;
const SkColor kBubbleColor = SK_ColorWHITE;

struct Line {
  int x1, y1, x2, y2;
};

// Line segments for the button icons. The line segments are painted in a 12x12
// centered box.
const Line kMinimizeLineSegments[] = {
  {1, 11, 11, 11}
};
const Line kMaximizeRestoreLineSegments[] = {
  {1, 1, 11, 1},
  {11, 1, 11, 11},
  {11, 11, 1, 11},
  {1, 11, 1, 1}
};
const Line kCloseLineSegments[] = {
  {1, 1, 11, 11},
  {1, 11, 11, 1}
};

// The amount that the origin of the icon's 12x12 box should be offset from the
// center of the button.
const int kIconOffsetFromCenter = -6;

// Sets |line_segments| to the line segments for the icon for |action|.
void GetLineSegmentsForAction(AlternateFrameCaptionButton::Action action,
                              const Line** line_segments,
                              size_t* num_line_segments) {
  switch(action) {
    case AlternateFrameCaptionButton::ACTION_MINIMIZE:
      *line_segments = kMinimizeLineSegments;
      *num_line_segments = arraysize(kMinimizeLineSegments);
      break;
    case AlternateFrameCaptionButton::ACTION_MAXIMIZE_RESTORE:
      *line_segments = kMaximizeRestoreLineSegments;
      *num_line_segments = arraysize(kMaximizeRestoreLineSegments);
      break;
    case AlternateFrameCaptionButton::ACTION_CLOSE:
      *line_segments = kCloseLineSegments;
      *num_line_segments = arraysize(kCloseLineSegments);
      break;
    default:
      NOTREACHED();
      break;
  }
}

}  // namespace

const char AlternateFrameCaptionButton::kViewClassName[] =
    "AlternateFrameCaptionButton";

AlternateFrameCaptionButton::AlternateFrameCaptionButton(
    views::ButtonListener* listener,
    Action action)
    : views::CustomButton(listener),
      action_(action),
      hidden_bubble_radius_(0),
      shown_bubble_radius_(0),
      bubble_animation_(new gfx::SlideAnimation(this)) {
}

AlternateFrameCaptionButton::~AlternateFrameCaptionButton() {
}

// static
int AlternateFrameCaptionButton::GetXOverlap() {
  return kFinalBurstBubbleRadius - kSize / 2;
}

gfx::Size AlternateFrameCaptionButton::GetPreferredSize() {
  gfx::Insets insets(GetInsets());
  return gfx::Size(
      std::max(kFinalBurstBubbleRadius * 2, kSize + insets.width()),
      kSize + insets.height());
}

const char* AlternateFrameCaptionButton::GetClassName() const {
  return kViewClassName;
}

bool AlternateFrameCaptionButton::HitTestRect(const gfx::Rect& rect) const {
  gfx::Rect bounds(GetLocalBounds());
  if (state_ == STATE_PRESSED)
    return bounds.Intersects(rect);

  int x_overlap = GetXOverlap();
  bounds.set_x(x_overlap);
  bounds.set_width(width() - x_overlap * 2);
  return bounds.Intersects(rect);
}

void AlternateFrameCaptionButton::OnPaint(gfx::Canvas* canvas) {
  gfx::Point content_bounds_center(GetContentsBounds().CenterPoint());

  int bubble_alpha = bubble_animation_->CurrentValueBetween(
      0, kShownBubbleOpacity);
  if (bubble_alpha != 0) {
    int bubble_radius = bubble_animation_->CurrentValueBetween(
        hidden_bubble_radius_, shown_bubble_radius_);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(SkColorSetA(kBubbleColor, bubble_alpha));
    canvas->DrawCircle(content_bounds_center, bubble_radius, paint);
  }

  SkColor color = kPathColor;
  if (state_ == STATE_HOVERED || state_ == STATE_PRESSED)
    color = kPressedHoveredPathColor;

  const Line* line_segments = NULL;
  size_t num_line_segments = 0;
  GetLineSegmentsForAction(action_, &line_segments, &num_line_segments);

  gfx::Vector2d top_left_offset(
      content_bounds_center.x() + kIconOffsetFromCenter,
      content_bounds_center.y() + kIconOffsetFromCenter);
  canvas->Translate(top_left_offset);
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(SkIntToScalar(2));
  paint.setStrokeCap(SkPaint::kSquare_Cap);
  paint.setColor(color);
  for (size_t i = 0; i < num_line_segments; ++i) {
    canvas->DrawLine(gfx::Point(line_segments[i].x1, line_segments[i].y1),
                     gfx::Point(line_segments[i].x2, line_segments[i].y2),
                     paint);
  }
  canvas->Translate(-top_left_offset);
}

void AlternateFrameCaptionButton::MaybeStartNewBubbleAnimation() {
  bool should_show = (state_ == STATE_PRESSED);
  if (should_show == bubble_animation_->IsShowing())
    return;

  if (!bubble_animation_->is_animating()) {
    if (should_show)
      hidden_bubble_radius_ = kInitialGrowBubbleRadius;
    else
      hidden_bubble_radius_ = kFinalBurstBubbleRadius;
    shown_bubble_radius_ = kFullyGrownBubbleRadius;

    bubble_animation_->SetSlideDuration(kBubbleAnimationDuration);
    if (should_show)
      bubble_animation_->Show();
    else
      bubble_animation_->Hide();
  } else {
    if (!should_show) {
      // The change in radius during a hide animation if there was no currently
      // running animation.
      int normal_radius_change =
          kFinalBurstBubbleRadius - kFullyGrownBubbleRadius;

      // Start a fade out animation from the bubble's current radius and
      // opacity. Update the bubble radius and opacity at the same rate that it
      // gets updated during a normal hide animation.
      int current_bubble_radius = bubble_animation_->CurrentValueBetween(
          kInitialGrowBubbleRadius, kFullyGrownBubbleRadius);
      hidden_bubble_radius_ = current_bubble_radius +
          bubble_animation_->GetCurrentValue() * normal_radius_change;
      shown_bubble_radius_ = hidden_bubble_radius_ - normal_radius_change;
      bubble_animation_->SetSlideDuration(
          bubble_animation_->GetCurrentValue() * kBubbleAnimationDuration);
      bubble_animation_->Hide();
    }
    // Else: The bubble is currently fading out. Wait till the hide animation
    // completes before starting an animation to show a new bubble.
  }
}

void AlternateFrameCaptionButton::StateChanged() {
  MaybeStartNewBubbleAnimation();
}

void AlternateFrameCaptionButton::AnimationProgressed(
    const gfx::Animation* animation) {
  SchedulePaint();
}

void AlternateFrameCaptionButton::AnimationEnded(
    const gfx::Animation* animation) {
  // The bubble animation was postponed if the button became pressed when the
  // bubble was fading out. Do the animation now.
  MaybeStartNewBubbleAnimation();
}

}  // namespace ash
