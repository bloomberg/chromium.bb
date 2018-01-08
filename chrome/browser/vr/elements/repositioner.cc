// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/repositioner.h"

#include "chrome/browser/vr/pose_util.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/quaternion.h"

namespace vr {

Repositioner::Repositioner(float content_depth)
    : content_depth_(content_depth) {
  set_hit_testable(false);
}

Repositioner::~Repositioner() = default;

gfx::Transform Repositioner::LocalTransform() const {
  return transform_;
}

gfx::Transform Repositioner::GetTargetLocalTransform() const {
  return transform_;
}

void Repositioner::UpdateTransform(const gfx::Transform& head_pose) {
  DCHECK(kOrigin.SquaredDistanceTo(laser_origin_) <
         content_depth_ * content_depth_);

  gfx::Vector3dF laser_origin_vector = laser_origin_ - kOrigin;
  float laser_origin_vector_length = laser_origin_vector.Length();
  gfx::Vector3dF normalized_laser_direction;
  bool success = laser_direction_.GetNormalized(&normalized_laser_direction);
  DCHECK(success);
  float d1 = -gfx::DotProduct(laser_origin_vector, normalized_laser_direction);
  float d2 = std::sqrt(
      content_depth_ * content_depth_ -
      (laser_origin_vector_length * laser_origin_vector_length - d1 * d1));
  float new_ray_distance = d1 + d2;
  gfx::Vector3dF new_ray =
      gfx::ScaleVector3d(normalized_laser_direction, new_ray_distance);
  gfx::Vector3dF new_reticle_vector = laser_origin_vector;
  new_reticle_vector.Add(new_ray);

  gfx::Vector3dF head_up_vector = vr::GetUpVector(head_pose);
  if (gfx::AngleBetweenVectorsInDegrees(head_up_vector, new_reticle_vector) ==
      0.f)
    return;

  transform_.MakeIdentity();

  gfx::Quaternion rotate_to_new_position({0, 0, -1}, new_reticle_vector);
  transform_.ConcatTransform(gfx::Transform(rotate_to_new_position));

  gfx::Vector3dF new_right_vector = {1, 0, 0};
  transform_.TransformVector(&new_right_vector);
  gfx::Vector3dF expected_right_vector =
      gfx::CrossProduct(new_reticle_vector, head_up_vector);

  gfx::Quaternion rotate_to_expected_right(new_right_vector,
                                           expected_right_vector);
  transform_.ConcatTransform(gfx::Transform(rotate_to_expected_right));
}

bool Repositioner::OnBeginFrame(const base::TimeTicks& time,
                                const gfx::Transform& head_pose) {
  if (enabled_) {
    UpdateTransform(head_pose);
    return true;
  }
  return false;
}

void Repositioner::DumpGeometry(std::ostringstream* os) const {
  gfx::Transform t = world_space_transform();
  gfx::Vector3dF forward = {0, 0, -1};
  t.TransformVector(&forward);
  // Decompose the rotation to world x axis followed by world y axis
  float x_rotation = std::asin(forward.y() / forward.Length());
  gfx::Vector3dF projected_forward = {forward.x(), 0, forward.z()};
  float y_rotation = std::acos(gfx::DotProduct(projected_forward, {0, 0, -1}) /
                               projected_forward.Length());
  if (projected_forward.x() > 0.f)
    y_rotation *= -1;
  *os << "rx(" << gfx::RadToDeg(x_rotation) << ") "
      << "ry(" << gfx::RadToDeg(y_rotation) << ") ";
}

}  // namespace vr
