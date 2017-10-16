// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/spinner.h"

#include "base/memory/ptr_util.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "cc/animation/transform_operations.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/vr/animation_player.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/target_property.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/gfx/canvas.h"

namespace vr {

static constexpr base::TimeDelta kSweepDuration =
    base::TimeDelta::FromSecondsD(2.0 / 3.0);
static constexpr base::TimeDelta kRotationDuration =
    base::TimeDelta::FromMilliseconds(1568);
static constexpr float kMinAngle = 0.0f;
static constexpr float kMaxAngle = 135.0f;
static constexpr float kThicknessFactor = 0.078125f;

std::unique_ptr<cc::CubicBezierTimingFunction> CreateTimingFunction() {
  return cc::CubicBezierTimingFunction::Create(0.4, 0.0, 0.2, 1.0);
}

class SpinnerTexture : public UiTexture {
 public:
  SpinnerTexture() {}
  ~SpinnerTexture() override {}

  void SetAngleSweep(float angle) {
    angle_sweep_ = angle;
    set_dirty();
  }

  void SetAngleStart(float angle) {
    angle_start_ = angle;
    set_dirty();
  }

  void SetRotation(float angle) {
    rotation_ = angle;
    set_dirty();
  }

  void SetColor(SkColor color) {
    color_ = color;
    set_dirty();
  }

 private:
  gfx::Size GetPreferredTextureSize(int width) const override {
    return gfx::Size(width, width);
  }

  gfx::SizeF GetDrawnSize() const override { return size_; }

  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override {
    float thickness = kThicknessFactor * texture_size.width();
    size_.set_height(texture_size.height());
    size_.set_width(texture_size.width());
    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeCap(SkPaint::kRound_Cap);
    paint.setColor(SK_ColorWHITE);
    paint.setStrokeWidth(thickness);
    float padding = thickness * 0.5f;
    sk_canvas->drawArc(
        SkRect::MakeLTRB(padding, padding, texture_size.width() - padding,
                         texture_size.height() - padding),
        angle_start_ - angle_sweep_ + rotation_, 2.0f * angle_sweep_, false,
        paint);
  }

  float angle_sweep_ = 0.0f;
  float angle_start_ = 0.0f;
  float rotation_ = 0.0f;
  gfx::SizeF size_;
  SkColor color_ = SK_ColorWHITE;

  DISALLOW_COPY_AND_ASSIGN(SpinnerTexture);
};

Spinner::Spinner(int maximum_width)
    : TexturedElement(maximum_width),
      texture_(base::MakeUnique<SpinnerTexture>()) {
  std::unique_ptr<cc::KeyframedFloatAnimationCurve> curve(
      cc::KeyframedFloatAnimationCurve::Create());

  curve->AddKeyframe(
      cc::FloatKeyframe::Create(base::TimeDelta(), 0.0f, nullptr));
  curve->AddKeyframe(
      cc::FloatKeyframe::Create(kRotationDuration, 360.0f, nullptr));

  std::unique_ptr<cc::Animation> animation(cc::Animation::Create(
      std::move(curve), AnimationPlayer::GetNextAnimationId(),
      AnimationPlayer::GetNextGroupId(), SPINNER_ROTATION));

  animation->set_iterations(-1);
  AddAnimation(std::move(animation));

  curve = cc::KeyframedFloatAnimationCurve::Create();

  for (size_t i = 0; i < 3; ++i) {
    curve->AddKeyframe(cc::FloatKeyframe::Create(kSweepDuration * i,
                                                 i % 2 ? kMaxAngle : kMinAngle,
                                                 CreateTimingFunction()));
  }

  animation = cc::Animation::Create(
      std::move(curve), AnimationPlayer::GetNextAnimationId(),
      AnimationPlayer::GetNextGroupId(), SPINNER_ANGLE_SWEEP);

  animation->set_iterations(-1);
  AddAnimation(std::move(animation));

  curve = cc::KeyframedFloatAnimationCurve::Create();

  for (size_t i = 0; i < 9; ++i) {
    curve->AddKeyframe(cc::FloatKeyframe::Create(
        kSweepDuration * i, kMaxAngle * i, CreateTimingFunction()));
  }

  animation = cc::Animation::Create(
      std::move(curve), AnimationPlayer::GetNextAnimationId(),
      AnimationPlayer::GetNextGroupId(), SPINNER_ANGLE_START);

  animation->set_iterations(-1);
  AddAnimation(std::move(animation));
}

Spinner::~Spinner() {}

void Spinner::SetColor(SkColor color) {
  texture_->SetColor(color);
}

UiTexture* Spinner::GetTexture() const {
  return texture_.get();
}

void Spinner::NotifyClientFloatAnimated(float value,
                                        int target_property_id,
                                        cc::Animation* animation) {
  switch (target_property_id) {
    case SPINNER_ANGLE_SWEEP:
      texture_->SetAngleSweep(value);
      break;
    case SPINNER_ANGLE_START:
      texture_->SetAngleStart(value);
      break;
    case SPINNER_ROTATION:
      texture_->SetRotation(value);
      break;
    default:
      TexturedElement::NotifyClientFloatAnimated(value, target_property_id,
                                                 animation);
  }
}

}  // namespace vr
