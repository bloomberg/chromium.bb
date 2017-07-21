// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/highlighter/highlighter_view.h"

#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
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

gfx::RectF GetPenTipRect(const gfx::PointF& p) {
  return gfx::RectF(p.x() - kPenTipWidth / 2, p.y() - kPenTipHeight / 2,
                    kPenTipWidth, kPenTipHeight);
}

gfx::Rect GetSegmentDamageRect(const gfx::RectF& r1, const gfx::RectF& r2) {
  gfx::RectF rect = r1;
  rect.Union(r2);
  rect.Inset(-kOutsetForAntialiasing, -kOutsetForAntialiasing);
  return gfx::ToEnclosingRect(rect);
}

// A highlighter segment is best imagined as a result of a rectangle
// being dragged along a straight line, while keeping its orientation.
// r1 is the start position of the rectangle and r2 is the final position.
void DrawSegment(gfx::Canvas& canvas,
                 const gfx::RectF& r1,
                 const gfx::RectF& r2,
                 const cc::PaintFlags& flags) {
  if (r1.x() > r2.x()) {
    DrawSegment(canvas, r2, r1, flags);
    return;
  }

  SkPath path;
  path.moveTo(r1.x(), r1.y());
  if (r1.y() < r2.y())
    path.lineTo(r1.right(), r1.y());
  else
    path.lineTo(r2.x(), r2.y());
  path.lineTo(r2.right(), r2.y());
  path.lineTo(r2.right(), r2.bottom());
  if (r1.y() < r2.y())
    path.lineTo(r2.x(), r2.bottom());
  else
    path.lineTo(r1.right(), r1.bottom());
  path.lineTo(r1.x(), r1.bottom());
  path.lineTo(r1.x(), r1.y());
  canvas.DrawPath(path, flags);
}

}  // namespace

const SkColor HighlighterView::kPenColor =
    SkColorSetA(kHighlighterColor, kHighlighterOpacity);

const gfx::SizeF HighlighterView::kPenTipSize(kPenTipWidth, kPenTipHeight);

HighlighterView::HighlighterView(aura::Window* root_window)
    : FastInkView(root_window) {}

HighlighterView::~HighlighterView() {}

void HighlighterView::AddNewPoint(const gfx::PointF& point) {
  TRACE_EVENT1("ui", "HighlighterView::AddNewPoint", "point", point.ToString());

  if (!points_.empty()) {
    UpdateDamageRect(GetSegmentDamageRect(GetPenTipRect(points_.back()),
                                          GetPenTipRect(point)));
  }

  points_.push_back(point);

  RequestRedraw();
}

void HighlighterView::Animate(const gfx::PointF& pivot,
                              AnimationMode animation_mode) {
  animation_timer_.reset(new base::Timer(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kStrokeFadeoutDelayMs),
      base::Bind(&HighlighterView::FadeOut, base::Unretained(this), pivot,
                 animation_mode),
      false));
  animation_timer_->Reset();
}

void HighlighterView::FadeOut(const gfx::PointF& pivot,
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
  if (points_.size() < 2)
    return;

  gfx::Rect clip_rect;
  if (!canvas.GetClipBounds(&clip_rect))
    return;

  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(kPenColor);
  flags.setBlendMode(SkBlendMode::kSrc);

  for (size_t i = 1; i < points_.size(); ++i) {
    const gfx::RectF tip1 = GetPenTipRect(points_[i - 1]) - offset;
    const gfx::RectF tip2 = GetPenTipRect(points_[i]) - offset;
    // Only draw the segment if it is touching the clip rect.
    if (clip_rect.Intersects(GetSegmentDamageRect(tip1, tip2)))
      DrawSegment(canvas, tip1, tip2, flags);
  }
}

}  // namespace ash
