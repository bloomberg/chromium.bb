// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/rotator/screen_rotation.h"

#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace ash {

namespace {

const int k90DegreeTransitionDurationMs = 350;
const int k180DegreeTransitionDurationMs = 550;
const int k360DegreeTransitionDurationMs = 750;

base::TimeDelta GetTransitionDuration(int degrees) {
  if (degrees == 360)
    return base::TimeDelta::FromMilliseconds(k360DegreeTransitionDurationMs);
  if (degrees == 180)
    return base::TimeDelta::FromMilliseconds(k180DegreeTransitionDurationMs);
  if (degrees == 0)
    return base::TimeDelta::FromMilliseconds(0);
  return base::TimeDelta::FromMilliseconds(k90DegreeTransitionDurationMs);
}

}  // namespace

ScreenRotation::ScreenRotation(int degrees, ui::Layer* layer)
    : ui::LayerAnimationElement(LayerAnimationElement::TRANSFORM,
                                GetTransitionDuration(degrees)),
      degrees_(degrees) {
  InitTransform(layer);
}

ScreenRotation::~ScreenRotation() {
}

void ScreenRotation::InitTransform(ui::Layer* layer) {
  // No rotation required, use the identity transform.
  if (degrees_ == 0) {
    interpolated_transform_.reset(
        new ui::InterpolatedConstantTransform(gfx::Transform()));
    return;
  }

  // Use the target transform/bounds in case the layer is already animating.
  const gfx::Transform& current_transform = layer->GetTargetTransform();
  const gfx::Rect& bounds = layer->GetTargetBounds();

  gfx::Point old_pivot;
  gfx::Point new_pivot;

  int width = bounds.width();
  int height = bounds.height();

  switch (degrees_) {
    case 90:
      new_origin_ = new_pivot = gfx::Point(width, 0);
      break;
    case -90:
      new_origin_ = new_pivot = gfx::Point(0, height);
      break;
    case 180:
    case 360:
      new_pivot = old_pivot = gfx::Point(width / 2, height / 2);
      new_origin_.SetPoint(width, height);
      break;
  }

  // Convert points to world space.
  current_transform.TransformPoint(&old_pivot);
  current_transform.TransformPoint(&new_pivot);
  current_transform.TransformPoint(&new_origin_);

  scoped_ptr<ui::InterpolatedTransform> rotation(
      new ui::InterpolatedTransformAboutPivot(
          old_pivot,
          new ui::InterpolatedRotation(0, degrees_)));

  scoped_ptr<ui::InterpolatedTransform> translation(
      new ui::InterpolatedTranslation(
          gfx::Point(0, 0),
          gfx::Point(new_pivot.x() - old_pivot.x(),
                     new_pivot.y() - old_pivot.y())));

  float scale_factor = 0.9f;
  scoped_ptr<ui::InterpolatedTransform> scale_down(
      new ui::InterpolatedScale(1.0f, scale_factor, 0.0f, 0.5f));

  scoped_ptr<ui::InterpolatedTransform> scale_up(
      new ui::InterpolatedScale(1.0f, 1.0f / scale_factor, 0.5f, 1.0f));

  interpolated_transform_.reset(
      new ui::InterpolatedConstantTransform(current_transform));

  scale_up->SetChild(scale_down.release());
  translation->SetChild(scale_up.release());
  rotation->SetChild(translation.release());
  interpolated_transform_->SetChild(rotation.release());
}

void ScreenRotation::OnStart(ui::LayerAnimationDelegate* delegate) {
}

bool ScreenRotation::OnProgress(double t,
                                ui::LayerAnimationDelegate* delegate) {
  delegate->SetTransformFromAnimation(interpolated_transform_->Interpolate(t));
  return true;
}

void ScreenRotation::OnGetTarget(TargetValue* target) const {
  target->transform = interpolated_transform_->Interpolate(1.0);
}

void ScreenRotation::OnAbort(ui::LayerAnimationDelegate* delegate) {
}

}  // namespace ash
