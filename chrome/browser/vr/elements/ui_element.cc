// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/ui_element.h"

#include <limits>

#include "base/logging.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "chrome/browser/vr/elements/ui_element_transform_operations.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace vr {

namespace {

int AllocateId() {
  static int g_next_id = 1;
  return g_next_id++;
}

bool GetRayPlaneDistance(const gfx::Point3F& ray_origin,
                         const gfx::Vector3dF& ray_vector,
                         const gfx::Point3F& plane_origin,
                         const gfx::Vector3dF& plane_normal,
                         float* distance) {
  float denom = gfx::DotProduct(-ray_vector, plane_normal);
  if (denom == 0) {
    return false;
  }
  gfx::Vector3dF rel = ray_origin - plane_origin;
  *distance = gfx::DotProduct(plane_normal, rel) / denom;
  return true;
}

}  // namespace

UiElement::UiElement() : id_(AllocateId()) {
  animation_player_.set_target(this);
  layout_offset_.AppendTranslate(0, 0, 0);
  transform_operations_.AppendTranslate(0, 0, 0);
  transform_operations_.AppendRotate(1, 0, 0, 0);
  transform_operations_.AppendScale(1, 1, 1);
}

UiElement::~UiElement() {
  animation_player_.set_target(nullptr);
}

void UiElement::Render(UiElementRenderer* renderer,
                       const gfx::Transform& model_view_proj_matrix) const {
  // Elements without an overridden implementation of Render should have their
  // draw phase set to kPhaseNone and should, consequently, be filtered out when
  // the UiRenderer collects elements to draw. Therefore, if we invoke this
  // function, it is an error.
  NOTREACHED();
}

void UiElement::Initialize() {}

void UiElement::OnHoverEnter(const gfx::PointF& position) {}

void UiElement::OnHoverLeave() {}

void UiElement::OnMove(const gfx::PointF& position) {}

void UiElement::OnButtonDown(const gfx::PointF& position) {}

void UiElement::OnButtonUp(const gfx::PointF& position) {}

void UiElement::OnFlingStart(std::unique_ptr<blink::WebGestureEvent> gesture,
                             const gfx::PointF& position) {}
void UiElement::OnFlingCancel(std::unique_ptr<blink::WebGestureEvent> gesture,
                              const gfx::PointF& position) {}
void UiElement::OnScrollBegin(std::unique_ptr<blink::WebGestureEvent> gesture,
                              const gfx::PointF& position) {}
void UiElement::OnScrollUpdate(std::unique_ptr<blink::WebGestureEvent> gesture,
                               const gfx::PointF& position) {}
void UiElement::OnScrollEnd(std::unique_ptr<blink::WebGestureEvent> gesture,
                            const gfx::PointF& position) {}

void UiElement::PrepareToDraw() {}

void UiElement::OnBeginFrame(const base::TimeTicks& time,
                             const gfx::Vector3dF& look_at) {
  animation_player_.Tick(time);
  last_frame_time_ = time;
}

bool UiElement::IsHitTestable() const {
  return IsVisible() && hit_testable_;
}

void UiElement::SetSize(float width, float height) {
  animation_player_.TransitionSizeTo(last_frame_time_, BOUNDS, size_,
                                     gfx::SizeF(width, height));
}

void UiElement::SetVisible(bool visible) {
  SetOpacity(visible ? opacity_when_visible_ : 0.0);
}

void UiElement::SetVisibleImmediately(bool visible) {
  opacity_ = visible ? opacity_when_visible_ : 0.0;
}

bool UiElement::IsVisible() const {
  return opacity() > 0.0f && computed_opacity() > 0.0f;
}

gfx::SizeF UiElement::size() const {
  DCHECK_LE(kUpdatedTexturesAndSizes, phase_);
  return size_;
}

void UiElement::SetTransformOperations(
    const UiElementTransformOperations& ui_element_transform_operations) {
  animation_player_.TransitionTransformOperationsTo(
      last_frame_time_, TRANSFORM, transform_operations_,
      ui_element_transform_operations.operations());
}

void UiElement::SetLayoutOffset(float x, float y) {
  cc::TransformOperations operations = layout_offset_;
  cc::TransformOperation& op = operations.at(0);
  op.translate = {x, y, 0};
  op.Bake();
  animation_player_.TransitionTransformOperationsTo(
      last_frame_time_, LAYOUT_OFFSET, transform_operations_, operations);
}

void UiElement::SetTranslate(float x, float y, float z) {
  cc::TransformOperations operations = transform_operations_;
  cc::TransformOperation& op = operations.at(kTranslateIndex);
  op.translate = {x, y, z};
  op.Bake();
  animation_player_.TransitionTransformOperationsTo(
      last_frame_time_, TRANSFORM, transform_operations_, operations);
}

void UiElement::SetRotate(float x, float y, float z, float radians) {
  cc::TransformOperations operations = transform_operations_;
  cc::TransformOperation& op = operations.at(kRotateIndex);
  op.rotate.axis = {x, y, z};
  op.rotate.angle = gfx::RadToDeg(radians);
  op.Bake();
  animation_player_.TransitionTransformOperationsTo(
      last_frame_time_, TRANSFORM, transform_operations_, operations);
}

void UiElement::SetScale(float x, float y, float z) {
  cc::TransformOperations operations = transform_operations_;
  cc::TransformOperation& op = operations.at(kScaleIndex);
  op.scale = {x, y, z};
  op.Bake();
  animation_player_.TransitionTransformOperationsTo(
      last_frame_time_, TRANSFORM, transform_operations_, operations);
}

void UiElement::SetOpacity(float opacity) {
  animation_player_.TransitionFloatTo(last_frame_time_, OPACITY, opacity_,
                                      opacity);
}

gfx::SizeF UiElement::GetTargetSize() const {
  return animation_player_.GetTargetSizeValue(TargetProperty::BOUNDS, size_);
}

cc::TransformOperations UiElement::GetTargetTransform() const {
  return animation_player_.GetTargetTransformOperationsValue(
      TargetProperty::TRANSFORM, transform_operations_);
}

float UiElement::GetTargetOpacity() const {
  return animation_player_.GetTargetFloatValue(TargetProperty::OPACITY,
                                               opacity_);
}

float UiElement::computed_opacity() const {
  DCHECK_LE(kUpdatedComputedOpacity, phase_);
  return computed_opacity_;
}

bool UiElement::HitTest(const gfx::PointF& point) const {
  return point.x() >= 0.0f && point.x() <= 1.0f && point.y() >= 0.0f &&
         point.y() <= 1.0f;
}

void UiElement::SetMode(ColorScheme::Mode mode) {
  for (auto& child : children_) {
    child->SetMode(mode);
  }
  if (mode_ == mode)
    return;
  mode_ = mode;
  OnSetMode();
}

const gfx::Transform& UiElement::world_space_transform() const {
  DCHECK_LE(kUpdatedWorldSpaceTransform, phase_);
  return world_space_transform_;
}

void UiElement::OnSetMode() {}
void UiElement::OnUpdatedWorldSpaceTransform() {}

gfx::SizeF UiElement::stale_size() const {
  DCHECK_LE(kUpdatedBindings, phase_);
  return size_;
}

void UiElement::AddChild(std::unique_ptr<UiElement> child) {
  child->parent_ = this;
  children_.push_back(std::move(child));
}

void UiElement::RemoveChild(UiElement* to_remove) {
  DCHECK_EQ(this, to_remove->parent_);
  to_remove->parent_ = nullptr;
  size_t old_size = children_.size();
  base::EraseIf(children_,
                [to_remove](const std::unique_ptr<UiElement>& child) {
                  return child.get() == to_remove;
                });
  DCHECK_NE(old_size, children_.size());
}

void UiElement::AddBinding(std::unique_ptr<BindingBase> binding) {
  bindings_.push_back(std::move(binding));
}

void UiElement::UpdateBindings() {
  for (auto& binding : bindings_)
    binding->Update();
}

gfx::Point3F UiElement::GetCenter() const {
  gfx::Point3F center;
  world_space_transform_.TransformPoint(&center);
  return center;
}

gfx::PointF UiElement::GetUnitRectangleCoordinates(
    const gfx::Point3F& world_point) const {
  // TODO(acondor): Simplify the math in this function.
  gfx::Point3F origin(0, 0, 0);
  gfx::Vector3dF x_axis(1, 0, 0);
  gfx::Vector3dF y_axis(0, 1, 0);
  world_space_transform_.TransformPoint(&origin);
  world_space_transform_.TransformVector(&x_axis);
  world_space_transform_.TransformVector(&y_axis);
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
  world_space_transform_.TransformVector(&x_axis);
  world_space_transform_.TransformVector(&y_axis);
  gfx::Vector3dF normal = CrossProduct(x_axis, y_axis);
  normal.GetNormalized(&normal);
  return normal;
}

bool UiElement::GetRayDistance(const gfx::Point3F& ray_origin,
                               const gfx::Vector3dF& ray_vector,
                               float* distance) const {
  return GetRayPlaneDistance(ray_origin, ray_vector, GetCenter(), GetNormal(),
                             distance);
}

void UiElement::NotifyClientFloatAnimated(float opacity,
                                          int target_property_id,
                                          cc::Animation* animation) {
  opacity_ = base::ClampToRange(opacity, 0.0f, 1.0f);
}

void UiElement::NotifyClientTransformOperationsAnimated(
    const cc::TransformOperations& operations,
    int target_property_id,
    cc::Animation* animation) {
  if (target_property_id == TRANSFORM) {
    transform_operations_ = operations;
  } else if (target_property_id == LAYOUT_OFFSET) {
    layout_offset_ = operations;
  } else {
    NOTREACHED();
  }
}

void UiElement::NotifyClientSizeAnimated(const gfx::SizeF& size,
                                         int target_property_id,
                                         cc::Animation* animation) {
  size_ = size;
}

void UiElement::SetTransitionedProperties(
    const std::set<TargetProperty>& properties) {
  std::set<int> converted_properties(properties.begin(), properties.end());
  animation_player_.SetTransitionedProperties(converted_properties);
}

void UiElement::AddAnimation(std::unique_ptr<cc::Animation> animation) {
  animation_player_.AddAnimation(std::move(animation));
}

void UiElement::RemoveAnimation(int animation_id) {
  animation_player_.RemoveAnimation(animation_id);
}

bool UiElement::IsAnimatingProperty(TargetProperty property) const {
  return animation_player_.IsAnimatingProperty(static_cast<int>(property));
}

void UiElement::LayOutChildren() {
  DCHECK_LE(kUpdatedTexturesAndSizes, phase_);
  for (auto& child : children_) {
    // To anchor a child, use the parent's size to find its edge.
    float x_offset;
    switch (child->x_anchoring()) {
      case XLEFT:
        x_offset = -0.5f * size().width();
        break;
      case XRIGHT:
        x_offset = 0.5f * size().width();
        break;
      case XNONE:
        x_offset = 0.0f;
        break;
    }
    float y_offset;
    switch (child->y_anchoring()) {
      case YTOP:
        y_offset = 0.5f * size().height();
        break;
      case YBOTTOM:
        y_offset = -0.5f * size().height();
        break;
      case YNONE:
        y_offset = 0.0f;
        break;
    }
    child->SetLayoutOffset(x_offset, y_offset);
  }
}

void UiElement::UpdateComputedOpacityRecursive() {
  set_computed_opacity(opacity_);
  if (parent_) {
    set_computed_opacity(computed_opacity_ * parent_->computed_opacity());
  }

  set_update_phase(kUpdatedComputedOpacity);

  for (auto& child : children_) {
    child->UpdateComputedOpacityRecursive();
  }
}

void UiElement::UpdateWorldSpaceTransformRecursive() {
  gfx::Transform transform;
  transform.Scale(size_.width(), size_.height());

  // Compute an inheritable transformation that can be applied to this element,
  // and it's children, if applicable.
  gfx::Transform inheritable = LocalTransform();

  if (parent_) {
    inheritable.ConcatTransform(parent_->inheritable_transform());
  }

  transform.ConcatTransform(inheritable);
  set_world_space_transform(transform);
  set_inheritable_transform(inheritable);
  set_update_phase(kUpdatedWorldSpaceTransform);

  for (auto& child : children_) {
    child->UpdateWorldSpaceTransformRecursive();
  }

  OnUpdatedWorldSpaceTransform();
}

gfx::Transform UiElement::LocalTransform() const {
  return layout_offset_.Apply() * transform_operations_.Apply();
}

}  // namespace vr
