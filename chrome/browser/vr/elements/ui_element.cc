// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/ui_element.h"

#include <limits>

#include "base/logging.h"
#include "base/time/time.h"
#include "cc/base/math_util.h"

namespace vr {

namespace {

static constexpr size_t kTranslateIndex = 0;
static constexpr size_t kRotateIndex = 1;
static constexpr size_t kScaleIndex = 2;

bool GetRayPlaneDistance(const gfx::Point3F& ray_origin,
                         const gfx::Vector3dF& ray_vector,
                         const gfx::Point3F& plane_origin,
                         const gfx::Vector3dF& plane_normal,
                         float* distance) {
  float denom = gfx::DotProduct(ray_vector, plane_normal);
  if (denom == 0) {
    return false;
  }
  gfx::Vector3dF rel = ray_origin - plane_origin;
  *distance = -gfx::DotProduct(plane_normal, rel) / denom;
  return true;
}

}  // namespace

UiElement::UiElement() {
  animation_player_.set_target(this);
  transform_operations_.AppendTranslate(0, 0, 0);
  transform_operations_.AppendRotate(1, 0, 0, 0);
  transform_operations_.AppendScale(1, 1, 1);
}

UiElement::~UiElement() {
  animation_player_.set_target(nullptr);
}

void UiElement::Render(UiElementRenderer* renderer,
                       gfx::Transform view_proj_matrix) const {
  NOTREACHED();
}

void UiElement::Initialize() {}

void UiElement::OnHoverEnter(const gfx::PointF& position) {}

void UiElement::OnHoverLeave() {}

void UiElement::OnMove(const gfx::PointF& position) {}

void UiElement::OnButtonDown(const gfx::PointF& position) {}

void UiElement::OnButtonUp(const gfx::PointF& position) {}

void UiElement::PrepareToDraw() {}

void UiElement::Animate(const base::TimeTicks& time) {
  animation_player_.Tick(time);
  last_frame_time_ = time;
}

bool UiElement::IsVisible() const {
  return visible_ && computed_opacity_ > 0.0f;
}

bool UiElement::IsHitTestable() const {
  return IsVisible() && hit_testable_;
}

void UiElement::SetEnabled(bool enabled) {
  visible_ = enabled;
}

void UiElement::SetSize(float width, float height) {
  animation_player_.TransitionBoundsTo(last_frame_time_, size_,
                                       gfx::SizeF(width, height));
}

void UiElement::SetTransformOperations(
    const cc::TransformOperations& operations) {
  DCHECK_EQ(3ul, operations.size());
  DCHECK_EQ(cc::TransformOperation::TRANSFORM_OPERATION_TRANSLATE,
            operations.at(kTranslateIndex).type);
  DCHECK_EQ(cc::TransformOperation::TRANSFORM_OPERATION_ROTATE,
            operations.at(kRotateIndex).type);
  DCHECK_EQ(cc::TransformOperation::TRANSFORM_OPERATION_SCALE,
            operations.at(kScaleIndex).type);
  animation_player_.TransitionTransformOperationsTo(
      last_frame_time_, transform_operations_, operations);
}

void UiElement::SetTranslate(float x, float y, float z) {
  cc::TransformOperations operations = transform_operations_;
  cc::TransformOperation& op = operations.at(kTranslateIndex);
  op.translate = {x, y, z};
  op.Bake();
  animation_player_.TransitionTransformOperationsTo(
      last_frame_time_, transform_operations_, operations);
}

void UiElement::SetRotate(float x, float y, float z, float radians) {
  cc::TransformOperations operations = transform_operations_;
  cc::TransformOperation& op = operations.at(kRotateIndex);
  op.rotate.axis = {x, y, z};
  op.rotate.angle = cc::MathUtil::Rad2Deg(radians);
  op.Bake();
  animation_player_.TransitionTransformOperationsTo(
      last_frame_time_, transform_operations_, operations);
}

void UiElement::SetScale(float x, float y, float z) {
  cc::TransformOperations operations = transform_operations_;
  cc::TransformOperation& op = operations.at(kScaleIndex);
  op.scale = {x, y, z};
  op.Bake();
  animation_player_.TransitionTransformOperationsTo(
      last_frame_time_, transform_operations_, operations);
}

void UiElement::SetOpacity(float opacity) {
  animation_player_.TransitionOpacityTo(last_frame_time_, opacity_, opacity);
}

bool UiElement::HitTest(const gfx::PointF& point) const {
  return point.x() >= 0.0f && point.x() <= 1.0f && point.y() >= 0.0f &&
         point.y() <= 1.0f;
}

void UiElement::SetMode(ColorScheme::Mode mode) {
  if (mode_ == mode)
    return;
  mode_ = mode;
  OnSetMode();
}

void UiElement::OnSetMode() {}

gfx::Point3F UiElement::GetCenter() const {
  gfx::Point3F center;
  screen_space_transform_.TransformPoint(&center);
  return center;
}

gfx::PointF UiElement::GetUnitRectangleCoordinates(
    const gfx::Point3F& world_point) const {
  // TODO(acondor): Simplify the math in this function.
  gfx::Point3F origin(0, 0, 0);
  gfx::Vector3dF x_axis(1, 0, 0);
  gfx::Vector3dF y_axis(0, 1, 0);
  screen_space_transform_.TransformPoint(&origin);
  screen_space_transform_.TransformVector(&x_axis);
  screen_space_transform_.TransformVector(&y_axis);
  gfx::Vector3dF origin_to_world = world_point - origin;
  float x = gfx::DotProduct(origin_to_world, x_axis) /
            gfx::DotProduct(x_axis, x_axis);
  float y = gfx::DotProduct(origin_to_world, y_axis) /
            gfx::DotProduct(y_axis, y_axis);
  return gfx::PointF(x, y);
}

gfx::Vector3dF UiElement::GetNormal() const {
  gfx::Vector3dF x_axis(1, 0, 0);
  gfx::Vector3dF y_axis(0, 1, 0);
  screen_space_transform_.TransformVector(&x_axis);
  screen_space_transform_.TransformVector(&y_axis);
  gfx::Vector3dF normal = CrossProduct(y_axis, x_axis);
  normal.GetNormalized(&normal);
  return normal;
}

bool UiElement::GetRayDistance(const gfx::Point3F& ray_origin,
                               const gfx::Vector3dF& ray_vector,
                               float* distance) const {
  return GetRayPlaneDistance(ray_origin, ray_vector, GetCenter(), GetNormal(),
                             distance);
}

void UiElement::NotifyClientOpacityAnimated(float opacity,
                                            cc::Animation* animation) {
  opacity_ = opacity;
}

void UiElement::NotifyClientTransformOperationsAnimated(
    const cc::TransformOperations& operations,
    cc::Animation* animation) {
  transform_operations_ = operations;
}

void UiElement::NotifyClientBoundsAnimated(const gfx::SizeF& size,
                                           cc::Animation* animation) {
  size_ = size;
}

}  // namespace vr
