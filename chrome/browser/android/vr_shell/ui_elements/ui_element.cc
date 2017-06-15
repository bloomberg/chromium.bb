// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_elements/ui_element.h"

#include <limits>

#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/android/vr_shell/animation.h"
#include "chrome/browser/android/vr_shell/easing.h"

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

gfx::Point3F WorldRectangle::GetCenter() const {
  gfx::Point3F center;
  transform_.TransformPoint(&center);
  return center;
}

gfx::PointF WorldRectangle::GetUnitRectangleCoordinates(
    const gfx::Point3F& world_point) const {
  // TODO(acondor): Simplify the math in this function.
  gfx::Point3F origin(0, 0, 0);
  gfx::Vector3dF x_axis(1, 0, 0);
  gfx::Vector3dF y_axis(0, 1, 0);
  transform_.TransformPoint(&origin);
  transform_.TransformVector(&x_axis);
  transform_.TransformVector(&y_axis);
  gfx::Vector3dF origin_to_world = world_point - origin;
  float x = gfx::DotProduct(origin_to_world, x_axis) /
            gfx::DotProduct(x_axis, x_axis);
  float y = gfx::DotProduct(origin_to_world, y_axis) /
            gfx::DotProduct(y_axis, y_axis);
  return gfx::PointF(x, y);
}

gfx::Vector3dF WorldRectangle::GetNormal() const {
  gfx::Vector3dF x_axis(1, 0, 0);
  gfx::Vector3dF y_axis(0, 1, 0);
  transform_.TransformVector(&x_axis);
  transform_.TransformVector(&y_axis);
  gfx::Vector3dF normal = CrossProduct(y_axis, x_axis);
  normal.GetNormalized(&normal);
  return normal;
}

bool WorldRectangle::GetRayDistance(const gfx::Point3F& ray_origin,
                                    const gfx::Vector3dF& ray_vector,
                                    float* distance) const {
  return GetRayPlaneDistance(ray_origin, ray_vector, GetCenter(), GetNormal(),
                             distance);
}

UiElement::UiElement() : rotation_(gfx::Vector3dF(1, 0, 0), 0) {}

UiElement::~UiElement() = default;

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
          animation.from.push_back(rotation_.x());
          animation.from.push_back(rotation_.y());
          animation.from.push_back(rotation_.z());
          animation.from.push_back(rotation_.w());
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
        rotation_.set_x(values[0]);
        rotation_.set_y(values[1]);
        rotation_.set_z(values[2]);
        rotation_.set_w(values[3]);
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

void UiElement::SetEnabled(bool enabled) {
  visible_ = enabled;
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

}  // namespace vr_shell
