// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/highlighter/highlighter_view.h"

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Variables for rendering the highlighter. Sizes in DIP.
const SkColor kHighlighterColor = SkColorSetRGB(0x42, 0x85, 0xF4);
constexpr int kHighlighterOpacity = 0xCC;
constexpr float kPenTipWidth = 4;
constexpr float kPenTipHeight = 14;
constexpr int kOutsetForAntialiasing = 1;

constexpr float kStrokeScale = 1.2;

constexpr int kStrokeFadeoutDelayMs = 500;
constexpr int kStrokeFadeoutDurationMs = 500;
constexpr int kStrokeScaleDurationMs = 300;

gfx::Rect InflateDamageRect(const gfx::Rect& r) {
  gfx::Rect inflated = r;
  inflated.Inset(
      -kOutsetForAntialiasing - static_cast<int>(kPenTipWidth / 2 + 1),
      -kOutsetForAntialiasing - static_cast<int>(kPenTipHeight / 2 + 1));
  return inflated;
}

// A highlighter segment is a parallelogram with two vertical sides.
// |p1| and |p2| are the center points of the vertical sides.
// |height| is the height of a vertical side.
void DrawSegment(gfx::Canvas& canvas,
                 const gfx::PointF& p1,
                 const gfx::PointF& p2,
                 int height,
                 const cc::PaintFlags& flags) {
  const float y_offset = height / 2;
  SkPath path;
  // When drawn with a thick round-joined outline, starting in the middle
  // of a vertical edge ensures smooth joining with the last edge.
  path.moveTo(p1.x(), p1.y());
  path.lineTo(p1.x(), p1.y() - y_offset);
  path.lineTo(p2.x(), p2.y() - y_offset);
  path.lineTo(p2.x(), p2.y() + y_offset);
  path.lineTo(p1.x(), p1.y() + y_offset);
  path.lineTo(p1.x(), p1.y() - y_offset);
  path.lineTo(p1.x(), p1.y());
  canvas.DrawPath(path, flags);
}

}  // namespace

const SkColor HighlighterView::kPenColor =
    SkColorSetA(kHighlighterColor, kHighlighterOpacity);

const gfx::SizeF HighlighterView::kPenTipSize(kPenTipWidth, kPenTipHeight);

HighlighterView::HighlighterView(base::TimeDelta presentation_delay,
                                 aura::Window* root_window)
    : FastInkView(root_window),
      points_(base::TimeDelta()),
      predicted_points_(base::TimeDelta()),
      presentation_delay_(presentation_delay) {}

HighlighterView::~HighlighterView() {}

void HighlighterView::AddNewPoint(const gfx::PointF& point,
                                  const base::TimeTicks& time) {
  TRACE_EVENT1("ui", "HighlighterView::AddNewPoint", "point", point.ToString());

  TRACE_COUNTER1(
      "ui", "HighlighterPredictionError",
      predicted_points_.GetNumberOfPoints()
          ? std::round(
                (point - predicted_points_.GetOldest().location).Length())
          : 0);
  // The new segment needs to be drawn.
  if (!points_.IsEmpty()) {
    UpdateDamageRect(InflateDamageRect(gfx::ToEnclosingRect(
        gfx::BoundingRect(points_.GetNewest().location, point))));
  }

  // Previous prediction needs to be erased.
  if (!predicted_points_.IsEmpty())
    UpdateDamageRect(InflateDamageRect(predicted_points_.GetBoundingBox()));

  points_.AddPoint(point, time);

  base::TimeTicks current_time = ui::EventTimeForNow();
  predicted_points_.Predict(
      points_, current_time, presentation_delay_,
      GetWidget()->GetNativeView()->GetBoundsInScreen().size());

  // New prediction needs to be drawn.
  if (!predicted_points_.IsEmpty())
    UpdateDamageRect(InflateDamageRect(predicted_points_.GetBoundingBox()));

  RequestRedraw();
}

void HighlighterView::Animate(const gfx::Point& pivot,
                              AnimationMode animation_mode) {
  animation_timer_.reset(new base::Timer(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kStrokeFadeoutDelayMs),
      base::Bind(&HighlighterView::FadeOut, base::Unretained(this), pivot,
                 animation_mode),
      false));
  animation_timer_->Reset();
}

void HighlighterView::FadeOut(const gfx::Point& pivot,
                              AnimationMode animation_mode) {
  ui::Layer* layer = GetWidget()->GetLayer();

  {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kStrokeFadeoutDurationMs));
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);

    layer->SetOpacity(0);
  }

  if (animation_mode != AnimationMode::kFadeout) {
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kStrokeScaleDurationMs));
    settings.SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);

    const float scale = animation_mode == AnimationMode::kInflate
                            ? kStrokeScale
                            : (1 / kStrokeScale);

    gfx::Transform transform;
    transform.Translate(pivot.x() * (1 - scale), pivot.y() * (1 - scale));
    transform.Scale(scale, scale);

    layer->SetTransform(transform);
  }
}

void HighlighterView::OnRedraw(gfx::Canvas& canvas,
                               const gfx::Vector2d& offset) {
  const int num_points =
      points_.GetNumberOfPoints() + predicted_points_.GetNumberOfPoints();
  if (num_points < 2)
    return;

  gfx::Rect clip_rect;
  if (!canvas.GetClipBounds(&clip_rect))
    return;

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStrokeAndFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(kPenColor);
  flags.setBlendMode(SkBlendMode::kSrc);
  flags.setStrokeWidth(kPenTipWidth);
  flags.setStrokeJoin(cc::PaintFlags::kRound_Join);

  // Decrease the segment height by the outline stroke width,
  // so that the vertical cross-section of the drawn segment
  // is exactly kPenTipHeight.
  const int height = kPenTipHeight - kPenTipWidth;

  gfx::PointF previous_point;

  for (int i = 0; i < num_points; ++i) {
    gfx::PointF current_point;
    if (i < points_.GetNumberOfPoints()) {
      current_point = points_.points()[i].location - offset;
    } else {
      current_point =
          predicted_points_.points()[i - points_.GetNumberOfPoints()].location -
          offset;
    }

    if (i != 0) {
      gfx::Rect damage_rect = InflateDamageRect(gfx::ToEnclosingRect(
          gfx::BoundingRect(previous_point, current_point)));
      // Only draw the segment if it is touching the clip rect.
      if (clip_rect.Intersects(damage_rect)) {
        DrawSegment(canvas, previous_point, current_point, height, flags);
      }
    }

    previous_point = current_point;
  }
}

}  // namespace ash
