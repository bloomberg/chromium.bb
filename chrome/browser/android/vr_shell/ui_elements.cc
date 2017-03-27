// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_elements.h"

#include <limits>

#include "base/logging.h"
#include "chrome/browser/android/vr_shell/animation.h"
#include "chrome/browser/android/vr_shell/easing.h"

namespace vr_shell {

namespace {

bool GetRayPlaneDistance(const gvr::Vec3f& ray_origin,
                         const gvr::Vec3f& ray_vector,
                         const gvr::Vec3f& plane_origin,
                         const gvr::Vec3f& plane_normal,
                         float* distance) {
  float denom = vr_shell::VectorDot(ray_vector, plane_normal);
  if (denom == 0) {
    return false;
  }
  gvr::Vec3f rel;
  rel.x = ray_origin.x - plane_origin.x;
  rel.y = ray_origin.y - plane_origin.y;
  rel.z = ray_origin.z - plane_origin.z;
  *distance = -vr_shell::VectorDot(plane_normal, rel) / denom;
  return true;
}

}  // namespace

Transform::Transform() {
  MakeIdentity();
}

void Transform::MakeIdentity() {
  SetIdentityM(to_world);
}

void Transform::Rotate(gvr::Quatf quat) {
  // TODO(klausw): use specialized rotation code? Constructing the matrix
  // via axis-angle quaternion is inefficient.
  gvr::Mat4f forward = QuatToMatrix(quat);
  to_world = MatrixMul(forward, to_world);
}

void Transform::Rotate(float ax, float ay, float az, float rad) {
  Rotate(QuatFromAxisAngle({ax, ay, az}, rad));
}

void Transform::Translate(float tx, float ty, float tz) {
  TranslateM(to_world, to_world, tx, ty, tz);
}

void Transform::Scale(float sx, float sy, float sz) {
  ScaleM(to_world, to_world, sx, sy, sz);
}

const gvr::Mat4f& WorldRectangle::TransformMatrix() const {
  return transform_.to_world;
}

gvr::Vec3f WorldRectangle::GetCenter() const {
  const gvr::Vec3f kOrigin = {0.0f, 0.0f, 0.0f};
  return MatrixVectorMul(transform_.to_world, kOrigin);
}

gvr::Vec2f WorldRectangle::GetUnitRectangleCoordinates(
    const gvr::Vec3f& world_point) {
  const gvr::Mat4f& transform = transform_.to_world;
  gvr::Vec3f origin = MatrixVectorMul(transform, gvr::Vec3f({0, 0, 0}));
  gvr::Vec3f xAxis = MatrixVectorMul(transform, gvr::Vec3f({1, 0, 0}));
  gvr::Vec3f yAxis = MatrixVectorMul(transform, gvr::Vec3f({0, 1, 0}));
  xAxis = VectorSubtract(xAxis, origin);
  yAxis = VectorSubtract(yAxis, origin);
  gvr::Vec3f point = VectorSubtract(world_point, origin);

  float x = VectorDot(point, xAxis) / VectorDot(xAxis, xAxis);
  float y = VectorDot(point, yAxis) / VectorDot(yAxis, yAxis);
  return {x, y};
}

gvr::Vec3f WorldRectangle::GetNormal() const {
  const gvr::Vec3f kNormalOrig = {0.0f, 0.0f, -1.0f};
  return MatrixVectorRotate(transform_.to_world, kNormalOrig);
}

bool WorldRectangle::GetRayDistance(const gvr::Vec3f& ray_origin,
                                    const gvr::Vec3f& ray_vector,
                                    float* distance) const {
  return GetRayPlaneDistance(ray_origin, ray_vector, GetCenter(), GetNormal(),
                             distance);
}

ContentRectangle::ContentRectangle() = default;

ContentRectangle::~ContentRectangle() = default;

void ContentRectangle::Animate(int64_t time) {
  for (auto& it : animations) {
    Animation& animation = *it;
    if (time < animation.start)
      continue;

    // If |from| is not specified, start at the current values.
    if (animation.from.size() == 0) {
      switch (animation.property) {
        case Animation::COPYRECT:
          animation.from.push_back(copy_rect.x);
          animation.from.push_back(copy_rect.y);
          animation.from.push_back(copy_rect.width);
          animation.from.push_back(copy_rect.height);
          break;
        case Animation::SIZE:
          animation.from.push_back(size.x);
          animation.from.push_back(size.y);
          break;
        case Animation::SCALE:
          animation.from.push_back(scale.x);
          animation.from.push_back(scale.y);
          animation.from.push_back(scale.z);
          break;
        case Animation::ROTATION:
          animation.from.push_back(rotation.x);
          animation.from.push_back(rotation.y);
          animation.from.push_back(rotation.z);
          animation.from.push_back(rotation.angle);
          break;
        case Animation::TRANSLATION:
          animation.from.push_back(translation.x);
          animation.from.push_back(translation.y);
          animation.from.push_back(translation.z);
          break;
        case Animation::OPACITY:
          animation.from.push_back(opacity);
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
          static_cast<double>(time - animation.start) / animation.duration);
      values[i] =
          animation.from[i] + (value * (animation.to[i] - animation.from[i]));
    }
    switch (animation.property) {
      case Animation::COPYRECT:
        CHECK_EQ(animation.from.size(), 4u);
        copy_rect.x = values[0];
        copy_rect.y = values[1];
        copy_rect.width = values[2];
        copy_rect.height = values[3];
        break;
      case Animation::SIZE:
        CHECK_EQ(animation.from.size(), 2u);
        size.x = values[0];
        size.y = values[1];
        break;
      case Animation::SCALE:
        CHECK_EQ(animation.from.size(), 3u);
        scale.x = values[0];
        scale.y = values[1];
        scale.z = values[2];
        break;
      case Animation::ROTATION:
        CHECK_EQ(animation.from.size(), 4u);
        rotation.x = values[0];
        rotation.y = values[1];
        rotation.z = values[2];
        rotation.angle = values[3];
        break;
      case Animation::TRANSLATION:
        CHECK_EQ(animation.from.size(), 3u);
        translation.x = values[0];
        translation.y = values[1];
        translation.z = values[2];
        break;
      case Animation::OPACITY:
        CHECK_EQ(animation.from.size(), 1u);
        opacity = values[0];
        break;
    }
  }
  for (auto it = animations.begin(); it != animations.end();) {
    const Animation& animation = **it;
    if (time >= (animation.start + animation.duration)) {
      it = animations.erase(it);
    } else {
      ++it;
    }
  }
}

bool ContentRectangle::IsVisible() const {
  return visible && computed_opacity > 0.0f;
}

bool ContentRectangle::IsHitTestable() const {
  return IsVisible() && hit_testable;
}

}  // namespace vr_shell
