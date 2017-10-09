// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/pin_keyboard_animation.h"

#include <memory>

#include "ui/compositor/layer_animation_delegate.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/interpolated_transform.h"

namespace ash {

PinKeyboardAnimation::PinKeyboardAnimation(bool grow,
                                           int height,
                                           base::TimeDelta duration,
                                           gfx::Tween::Type tween_type)
    : ui::LayerAnimationElement(
          LayerAnimationElement::TRANSFORM | LayerAnimationElement::OPACITY,
          duration),
      tween_type_(tween_type) {
  if (!grow)
    std::swap(start_opacity_, end_opacity_);

  gfx::Transform to_center;
  to_center.Translate(0, height / 2.f);
  auto move_to_center =
      std::make_unique<ui::InterpolatedConstantTransform>(to_center);

  auto scale = std::make_unique<ui::InterpolatedScale>(
      gfx::Point3F(1, start_opacity_, 1), gfx::Point3F(1, end_opacity_, 1));

  gfx::Transform from_center;
  from_center.Translate(0, -height / 2.f);
  auto move_from_center =
      std::make_unique<ui::InterpolatedConstantTransform>(from_center);

  scale->SetChild(std::move(move_to_center));
  move_from_center->SetChild(std::move(scale));
  transform_ = std::move(move_from_center);
}

PinKeyboardAnimation::~PinKeyboardAnimation() {}

void PinKeyboardAnimation::OnStart(ui::LayerAnimationDelegate* delegate) {}

bool PinKeyboardAnimation::OnProgress(double current,
                                      ui::LayerAnimationDelegate* delegate) {
  const double tweened = gfx::Tween::CalculateValue(tween_type_, current);
  delegate->SetOpacityFromAnimation(
      gfx::Tween::FloatValueBetween(tweened, start_opacity_, end_opacity_));
  delegate->SetTransformFromAnimation(transform_->Interpolate(tweened));
  return true;
}

void PinKeyboardAnimation::OnGetTarget(TargetValue* target) const {}

void PinKeyboardAnimation::OnAbort(ui::LayerAnimationDelegate* delegate) {}

}  // namespace ash
