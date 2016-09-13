// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_elements.h"

#include <cmath>
#include <vector>

namespace vr_shell {

namespace {

float GetRayPlaneIntersection(gvr::Vec3f ray_origin,
                              gvr::Vec3f ray_vector,
                              gvr::Vec3f plane_origin,
                              gvr::Vec3f plane_normal) {
  float denom = vr_shell::VectorDot(ray_vector, plane_normal);
  if (denom == 0) {
    // TODO(mthiesse): Line could be contained in the plane, do we care?
    return -std::numeric_limits<float>::infinity();
  }
  gvr::Vec3f rel;
  rel.x = ray_origin.x - plane_origin.x;
  rel.y = ray_origin.y - plane_origin.y;
  rel.z = ray_origin.z - plane_origin.z;

  return -vr_shell::VectorDot(plane_normal, rel) / denom;
}

}  // namespace

ReversibleTransform::ReversibleTransform() { MakeIdentity(); }

void ReversibleTransform::MakeIdentity() {
  SetIdentityM(to_world);
  SetIdentityM(from_world);
  orientation.qx = orientation.qy = orientation.qz = 0.0f;
  orientation.qw = 1.0f;
}

void ReversibleTransform::Rotate(gvr::Quatf quat) {
  orientation = QuatMultiply(quat, orientation);

  // TODO(klausw): use specialized rotation code? Constructing the matrix
  // via axis-angle quaternion is inefficient.
  gvr::Mat4f forward = QuatToMatrix(quat);
  to_world = MatrixMul(forward, to_world);
  gvr::Mat4f reverse = MatrixTranspose(forward);
  from_world = MatrixMul(from_world, reverse);
}

void ReversibleTransform::Rotate(float ax, float ay, float az, float rad) {
  // TODO(klausw): use specialized rotation code? Constructing the matrix
  // via axis-angle quaternion is inefficient.
  Rotate(QuatFromAxisAngle(ax, ay, az, rad));
}

void ReversibleTransform::Translate(float tx, float ty, float tz) {
  TranslateM(to_world, to_world, tx, ty, tz);
  TranslateMRight(from_world, from_world, -tx, -ty, -tz);
}

void ReversibleTransform::Scale(float sx, float sy, float sz) {
  ScaleM(to_world, to_world, sx, sy, sz);
  ScaleMRight(from_world, from_world, 1.0f / sx, 1.0f / sy, 1.0f / sz);
}

gvr::Vec3f WorldRectangle::GetCenter() const {
  const gvr::Vec3f kOrigin = {0.0f, 0.0f, 0.0f};
  return MatrixVectorMul(transform.to_world, kOrigin);
}

gvr::Vec3f WorldRectangle::GetNormal() const {
  const gvr::Vec3f kNormalOrig = {0.0f, 0.0f, -1.0f};
  return MatrixVectorRotate(transform.to_world, kNormalOrig);
}

float WorldRectangle::GetRayDistance(gvr::Vec3f ray_origin,
                                     gvr::Vec3f ray_vector) const {
  return GetRayPlaneIntersection(ray_origin, ray_vector, GetCenter(),
                                 GetNormal());
}

ContentRectangle::ContentRectangle() = default;

ContentRectangle::~ContentRectangle() = default;

}  // namespace vr_shell

