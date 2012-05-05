// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/magnification_controller.h"

#include "ash/shell.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_property.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace {

const float kMaximumMagnifiScale = 4.0f;
const float kMaximumMagnifiScaleThreshold = 4.0f;
const float kMinimumMagnifiScale = 1.0f;
const float kMinimumMagnifiScaleThreshold = 1.1f;

}  // namespace

namespace ash {
namespace internal {

MagnificationController::MagnificationController()
    : scale_(1.0f), x_(0), y_(0) {
  root_window_ = ash::Shell::GetRootWindow();
}

void MagnificationController::SetScale(float scale) {
  scale_ = scale;
  RedrawScreen(true);
}

void MagnificationController::MoveWindow(int x, int y) {
  y_ = y;
  x_ = x;
  RedrawScreen(true);
}

void MagnificationController::MoveWindow(const gfx::Point& point) {
  MoveWindow(point.x(), point.y());
}

void MagnificationController::EnsureShowRect(const gfx::Rect& target_rect) {
  gfx::Rect rect = GetWindowRect().AdjustToFit(target_rect);
  MoveWindow(rect.x(), rect.y());
}

void MagnificationController::EnsureShowPoint(const gfx::Point& point,
                                              bool animation) {
  gfx::Rect rect = GetWindowRect();

  if (rect.Contains(point))
    return;

  if (point.x() < rect.x())
    x_ = point.x();
  else if(rect.right() < point.x())
    x_ = point.x() - rect.width();

  if (point.y() < rect.y())
    y_ = point.y();
  else if(rect.bottom() < point.y())
    y_ = point.y() - rect.height();

  RedrawScreen(animation);
}

void MagnificationController::RedrawScreen(bool animation) {

  // Adjust the scale to just |kMinimumMagnifiScale| if scale is smaller than
  // |kMinimumMagnifiScaleThreshold|;
  if (scale_ < kMinimumMagnifiScaleThreshold)
    scale_ = kMinimumMagnifiScale;
  // Adjust the scale to just |kMinimumMagnifiScale| if scale is bigger than
  // |kMinimumMagnifiScaleThreshold|;
  if (scale_ > kMaximumMagnifiScaleThreshold)
    scale_ = kMaximumMagnifiScale;

  if (x_ < 0)
    x_ = 0;
  if (y_ < 0)
    y_ = 0;

  gfx::Size host_size = root_window_->GetHostSize();
  gfx::Size window_size = GetWindowRect().size();
  int max_x = host_size.width() - window_size.width();
  int max_y = host_size.height() - window_size.height();
  if (x_ > max_x)
    x_ = max_x;
  if (y_ > max_y)
    y_ = max_y;


  float scale = scale_;
  int x = x_;
  int y = y_;

  // Creates transform matrix.
  ui::Transform transform;
  // Flips the signs intentionally to convert them from the position of the
  // magnification window.
  transform.ConcatTranslate(-x, -y);
  transform.ConcatScale(scale, scale);

  if (animation) {
    ui::ScopedLayerAnimationSettings settings(
        root_window_->layer()->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
    settings.SetTweenType(ui::Tween::EASE_IN_OUT);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(100));
  }

  root_window_->layer()->SetTransform(transform);
}

gfx::Rect MagnificationController::GetWindowRect() {
  gfx::Size size = root_window_->GetHostSize();
  int width = size.width() / scale_;
  int height = size.height() / scale_;

  return gfx::Rect(x_, y_, width, height);
}

}  // namespace internal
}  // namespace ash
