// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_elements/ui_element.h"

#include <limits>

#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/android/vr_shell/animation.h"
#include "chrome/browser/android/vr_shell/easing.h"
#include "device/vr/vr_math.h"

namespace vr_shell {

namespace {

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

Transform::Transform() {
  MakeIdentity();
}

Transform::Transform(const Transform& other) {
  to_world = other.to_world;
}

void Transform::MakeIdentity() {
  vr::SetIdentityM(&to_world);
}

void Transform::Rotate(const vr::Quatf& quat) {
  // TODO(klausw): use specialized rotation code? Constructing the matrix
  // via axis-angle quaternion is inefficient.
  vr::Mat4f forward;
  vr::QuatToMatrix(quat, &forward);
  vr::MatrixMul(forward, to_world, &to_world);
}

void Transform::Rotate(const vr::RotationAxisAngle& axis_angle) {
  Rotate(vr::QuatFromAxisAngle(axis_angle));
}

void Transform::Translate(const gfx::Vector3dF& translation) {
  vr::TranslateM(to_world, translation, &to_world);
}

void Transform::Scale(const gfx::Vector3dF& scale) {
  vr::ScaleM(to_world, scale, &to_world);
}

const vr::Mat4f& WorldRectangle::TransformMatrix() const {
  return transform_.to_world;
}

gfx::Point3F WorldRectangle::GetCenter() const {
  const gfx::Point3F kOrigin(0.0f, 0.0f, 0.0f);
  return kOrigin + vr::GetTranslation(transform_.to_world);
}

gfx::PointF WorldRectangle::GetUnitRectangleCoordinates(
    const gfx::Point3F& world_point) const {
  // TODO(acondor): Simplify the math in this function.
  const vr::Mat4f& transform = transform_.to_world;
  gfx::Vector3dF origin =
      vr::MatrixVectorMul(transform, gfx::Vector3dF(0, 0, 0));
  gfx::Vector3dF x_axis =
      vr::MatrixVectorMul(transform, gfx::Vector3dF(1, 0, 0));
  gfx::Vector3dF y_axis =
      vr::MatrixVectorMul(transform, gfx::Vector3dF(0, 1, 0));
  x_axis.Subtract(origin);
  y_axis.Subtract(origin);
  gfx::Point3F point = world_point - origin;
  gfx::Vector3dF v_point(point.x(), point.y(), point.z());

  float x = gfx::DotProduct(v_point, x_axis) / gfx::DotProduct(x_axis, x_axis);
  float y = gfx::DotProduct(v_point, y_axis) / gfx::DotProduct(y_axis, y_axis);
  return gfx::PointF(x, y);
}

gfx::Vector3dF WorldRectangle::GetNormal() const {
  const gfx::Vector3dF kNormalOrig = {0.0f, 0.0f, -1.0f};
  return vr::MatrixVectorRotate(transform_.to_world, kNormalOrig);
}

bool WorldRectangle::GetRayDistance(const gfx::Point3F& ray_origin,
                                    const gfx::Vector3dF& ray_vector,
                                    float* distance) const {
  return GetRayPlaneDistance(ray_origin, ray_vector, GetCenter(), GetNormal(),
                             distance);
}

UiElement::UiElement() = default;

UiElement::~UiElement() = default;

void UiElement::Render(VrShellRenderer* renderer,
                       vr::Mat4f view_proj_matrix) const {
  NOTREACHED();
}

void UiElement::Initialize() {}

void UiElement::OnHoverEnter(gfx::PointF position) {}

void UiElement::OnHoverLeave() {}

void UiElement::OnMove(gfx::PointF position) {}

void UiElement::OnButtonDown(gfx::PointF position) {}

void UiElement::OnButtonUp(gfx::PointF position) {}

void UiElement::OnBeginFrame(const base::TimeTicks& begin_frame_time) {}

void UiElement::Animate(const base::TimeTicks& time) {
  for (auto& it : animations_) {
    Animation& animation = *it;
    if (time < animation.start)
      continue;

    // If |from| is not specified, start at the current values.
    if (animation.from.size() == 0) {
      switch (animation.property) {
        case Animation::SIZE:
          animation.from.push_back(size_.x());
          animation.from.push_back(size_.y());
          break;
        case Animation::SCALE:
          animation.from.push_back(scale_.x());
          animation.from.push_back(scale_.y());
          animation.from.push_back(scale_.z());
          break;
        case Animation::ROTATION:
          animation.from.push_back(rotation_.x);
          animation.from.push_back(rotation_.y);
          animation.from.push_back(rotation_.z);
          animation.from.push_back(rotation_.angle);
          break;
        case Animation::TRANSLATION:
          animation.from.push_back(translation_.x());
          animation.from.push_back(translation_.y());
          animation.from.push_back(translation_.z());
          break;
        case Animation::OPACITY:
          animation.from.push_back(opacity_);
          break;
      }
    }
    CHECK_EQ(animation.from.size(), animation.to.size());

    std::vector<float> values(animation.from.size());
    for (std::size_t i = 0; i < animation.from.size(); ++i) {
      if (animation.to[i] == animation.from[i] ||
          time >= (animation.start + animation.duration)) {
        values[i] = animation.to[i];
        continue;
      }
      double value = animation.easing->CalculateValue(
          (time - animation.start).InMillisecondsF() /
          animation.duration.InMillisecondsF());
      values[i] =
          animation.from[i] + (value * (animation.to[i] - animation.from[i]));
    }
    switch (animation.property) {
      case Animation::SIZE:
        CHECK_EQ(animation.from.size(), 2u);
        size_.set_x(values[0]);
        size_.set_y(values[1]);
        break;
      case Animation::SCALE:
        CHECK_EQ(animation.from.size(), 3u);
        scale_ = {values[0], values[1], values[2]};
        break;
      case Animation::ROTATION:
        CHECK_EQ(animation.from.size(), 4u);
        rotation_.x = values[0];
        rotation_.y = values[1];
        rotation_.z = values[2];
        rotation_.angle = values[3];
        break;
      case Animation::TRANSLATION:
        CHECK_EQ(animation.from.size(), 3u);
        translation_ = {values[0], values[1], values[2]};
        break;
      case Animation::OPACITY:
        CHECK_EQ(animation.from.size(), 1u);
        opacity_ = values[0];
        break;
    }
  }
  for (auto it = animations_.begin(); it != animations_.end();) {
    const Animation& animation = **it;
    if (time >= (animation.start + animation.duration)) {
      it = animations_.erase(it);
    } else {
      ++it;
    }
  }
}

bool UiElement::IsVisible() const {
  return visible_ && computed_opacity_ > 0.0f;
}

bool UiElement::IsHitTestable() const {
  return IsVisible() && hit_testable_;
}

}  // namespace vr_shell
