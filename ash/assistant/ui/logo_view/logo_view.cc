// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/logo_view/logo_view.h"

#include <algorithm>

#include "ash/assistant/ui/logo_view/shape/shape.h"
#include "base/logging.h"
#include "chromeos/assistant/internal/logo_view/logo_model/dot.h"
#include "chromeos/assistant/internal/logo_view/logo_view_constants.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ash {

namespace {

constexpr float kDotsScale = 1.0f;

int64_t TimeTicksToMs(const base::TimeTicks& timestamp) {
  return (timestamp - base::TimeTicks()).InMilliseconds();
}

}  // namespace

LogoView::LogoView()
    : mic_part_shape_(chromeos::assistant::kDotDefaultSize),
      state_animator_(&logo_, &state_model_, StateModel::State::kUndefined) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  state_animator_.SetStateAnimatorTimerDelegate(this);
}

LogoView::~LogoView() {
  state_animator_.StopAnimator();
}

void LogoView::SwitchStateTo(StateModel::State state, bool animate) {
  state_animator_.SwitchStateTo(state, !animate);
}

int64_t LogoView::StartTimer() {
  ui::Compositor* compositor = layer()->GetCompositor();
  if (compositor && !compositor->HasAnimationObserver(this)) {
    animating_compositor_ = compositor;
    animating_compositor_->AddAnimationObserver(this);
  }
  return TimeTicksToMs(base::TimeTicks::Now());
}

void LogoView::StopTimer() {
  if (animating_compositor_ &&
      animating_compositor_->HasAnimationObserver(this)) {
    animating_compositor_->RemoveAnimationObserver(this);
  }
  animating_compositor_ = nullptr;
}

void LogoView::OnAnimationStep(base::TimeTicks timestamp) {
  const int64_t current_time_ms = TimeTicksToMs(timestamp);
  state_animator_.OnTimeUpdate(current_time_ms);
  SchedulePaint();
}

void LogoView::OnCompositingShuttingDown(ui::Compositor* compositor) {
  DCHECK(compositor);
  if (animating_compositor_ == compositor)
    StopTimer();
}

void LogoView::DrawDots(gfx::Canvas* canvas) {
  // TODO: The Green Mic parts seems overlapped on the Red Mic part. Draw dots
  // in reverse order so that the Red Mic part is on top of Green Mic parts. But
  // we need to find out why the Mic parts are overlapping in the first place.
  for (auto iter = logo_.dots().rbegin(); iter != logo_.dots().rend(); ++iter)
    DrawDot(canvas, (*iter).get());

  layer()->SetOpacity(logo_.GetAlpha());
}

void LogoView::DrawDot(gfx::Canvas* canvas, Dot* dot) {
  const float radius = dot->GetRadius();
  const float angle = logo_.GetRotation() + dot->GetAngle();
  const float x = radius * std::cos(angle) + dot->GetOffsetX();
  const float y = radius * std::sin(angle) + dot->GetOffsetY();

  if (dot->IsMic()) {
    DrawMicPart(canvas, dot, x, y);
  } else if (dot->IsLetter()) {
    // TODO(b/79579731): Implement the letter animation.
    NOTIMPLEMENTED();
  } else if (dot->IsLine()) {
    DrawLine(canvas, dot, x, y);
  } else {
    DrawCircle(canvas, dot, x, y);
  }
}

void LogoView::DrawMicPart(gfx::Canvas* canvas, Dot* dot, float x, float y) {
  const float progress = dot->GetMicMorph();
  mic_part_shape_.Reset();
  mic_part_shape_.ToMicPart(progress, dot->dot_color());
  mic_part_shape_.Transform(x, y, kDotsScale);
  DrawShape(canvas, &mic_part_shape_, dot->color());
}

void LogoView::DrawShape(gfx::Canvas* canvas, Shape* shape, SkColor color) {
  cc::PaintFlags paint_flags;
  paint_flags.setAntiAlias(true);
  paint_flags.setColor(color);
  paint_flags.setStyle(cc::PaintFlags::kStroke_Style);
  paint_flags.setStrokeCap(shape->cap());

  paint_flags.setStrokeWidth(shape->first_stroke_width());
  canvas->DrawPath(*shape->first_path(), paint_flags);

  paint_flags.setStrokeWidth(shape->second_stroke_width());
  canvas->DrawPath(*shape->second_path(), paint_flags);
}

void LogoView::DrawLine(gfx::Canvas* canvas, Dot* dot, float x, float y) {
  const float stroke_width = dot->GetSize() * kDotsScale;
  cc::PaintFlags paint_flags;
  paint_flags.setAntiAlias(true);
  paint_flags.setColor(dot->color());
  paint_flags.setStrokeWidth(stroke_width);
  paint_flags.setStyle(cc::PaintFlags::kStroke_Style);
  paint_flags.setStrokeCap(cc::PaintFlags::kRound_Cap);

  const float line_length = dot->GetLineLength();
  const float line_x = x * kDotsScale;
  const float line_top_y = (y - line_length) * kDotsScale;
  const float line_bottom_y = (y + line_length) * kDotsScale;
  canvas->DrawLine(gfx::PointF(line_x, line_top_y),
                   gfx::PointF(line_x, line_bottom_y), paint_flags);
}

void LogoView::DrawCircle(gfx::Canvas* canvas, Dot* dot, float x, float y) {
  const float radius = dot->GetSize() * dot->GetVisibility() / 2.0f;
  cc::PaintFlags paint_flags;
  paint_flags.setAntiAlias(true);
  paint_flags.setColor(dot->color());
  paint_flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(gfx::PointF(x * kDotsScale, y * kDotsScale),
                     radius * kDotsScale, paint_flags);
}

void LogoView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  canvas->Save();
  gfx::RectF content_bounds(GetContentsBounds());
  gfx::InsetsF insets(GetInsets());
  const float offset_x = insets.left() + content_bounds.width() / 2.0f;
  const float offset_y = insets.top() + content_bounds.height() / 2.0f;
  canvas->Translate(gfx::Vector2d(offset_x, offset_y));
  DrawDots(canvas);
  canvas->Restore();
}

void LogoView::VisibilityChanged(views::View* starting_from, bool is_visible) {
  if (is_visible)
    state_animator_.StartAnimator();
  else
    state_animator_.StopAnimator();
}

}  // namespace ash
