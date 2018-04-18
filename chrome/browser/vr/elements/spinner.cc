// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/spinner.h"

#include "cc/animation/keyframed_animation_curve.h"
#include "cc/animation/timing_function.h"
#include "cc/animation/transform_operations.h"
#include "cc/paint/skia_paint_canvas.h"
#include "chrome/browser/vr/animation.h"
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

  void SetAngleSweep(float angle) { SetAndDirty(&angle_sweep_, angle); }
  void SetAngleStart(float angle) { SetAndDirty(&angle_start_, angle); }
  void SetRotation(float angle) { SetAndDirty(&rotation_, angle); }
  void SetColor(SkColor color) { SetAndDirty(&color_, color); }

 private:
  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override {
    float thickness = kThicknessFactor * texture_size.width();
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
  SkColor color_ = SK_ColorWHITE;

  DISALLOW_COPY_AND_ASSIGN(SpinnerTexture);
};

Spinner::Spinner(int texture_width)
    : TexturedElement(),
      texture_(std::make_unique<SpinnerTexture>()),
      texture_width_(texture_width) {
  std::unique_ptr<cc::KeyframedFloatAnimationCurve> curve(
      cc::KeyframedFloatAnimationCurve::Create());

  curve->AddKeyframe(
      cc::FloatKeyframe::Create(base::TimeDelta(), 0.0f, nullptr));
  curve->AddKeyframe(
      cc::FloatKeyframe::Create(kRotationDuration, 360.0f, nullptr));

  std::unique_ptr<cc::KeyframeModel> keyframe_model(cc::KeyframeModel::Create(
      std::move(curve), Animation::GetNextKeyframeModelId(),
      Animation::GetNextGroupId(), SPINNER_ROTATION));

  keyframe_model->set_iterations(-1);
  AddKeyframeModel(std::move(keyframe_model));

  curve = cc::KeyframedFloatAnimationCurve::Create();

  for (size_t i = 0; i < 3; ++i) {
    curve->AddKeyframe(cc::FloatKeyframe::Create(kSweepDuration * i,
                                                 i % 2 ? kMaxAngle : kMinAngle,
                                                 CreateTimingFunction()));
  }

  keyframe_model = cc::KeyframeModel::Create(
      std::move(curve), Animation::GetNextKeyframeModelId(),
      Animation::GetNextGroupId(), SPINNER_ANGLE_SWEEP);

  keyframe_model->set_iterations(-1);
  AddKeyframeModel(std::move(keyframe_model));

  curve = cc::KeyframedFloatAnimationCurve::Create();

  for (size_t i = 0; i < 9; ++i) {
    curve->AddKeyframe(cc::FloatKeyframe::Create(
        kSweepDuration * i, kMaxAngle * i, CreateTimingFunction()));
  }

  keyframe_model = cc::KeyframeModel::Create(
      std::move(curve), Animation::GetNextKeyframeModelId(),
      Animation::GetNextGroupId(), SPINNER_ANGLE_START);

  keyframe_model->set_iterations(-1);
  AddKeyframeModel(std::move(keyframe_model));
}

Spinner::~Spinner() {}

void Spinner::SetColor(SkColor color) {
  texture_->SetColor(color);
}

UiTexture* Spinner::GetTexture() const {
  return texture_.get();
}

bool Spinner::TextureDependsOnMeasurement() const {
  return false;
}

gfx::Size Spinner::MeasureTextureSize() {
  return gfx::Size(texture_width_, texture_width_);
}

void Spinner::NotifyClientFloatAnimated(float value,
                                        int target_property_id,
                                        cc::KeyframeModel* keyframe_model) {
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
                                                 keyframe_model);
  }
}

}  // namespace vr
