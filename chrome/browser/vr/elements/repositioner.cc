// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/repositioner.h"

#include "chrome/browser/vr/pose_util.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/quaternion.h"

namespace vr {

namespace {

constexpr float kSnapThresholdDegrees = 10.0f;

}  // namespace

Repositioner::Repositioner() {
  set_hit_testable(false);
}

Repositioner::~Repositioner() = default;

gfx::Transform Repositioner::LocalTransform() const {
  return transform_;
}

gfx::Transform Repositioner::GetTargetLocalTransform() const {
  return transform_;
}

void Repositioner::SetEnabled(bool enabled) {
  bool check_for_snap = !enabled && enabled_;
  enabled_ = enabled;
  if (!check_for_snap)
    return;

  gfx::Vector3dF world_up = {0, 1, 0};
  gfx::Vector3dF up = world_up;
  transform_.TransformVector(&up);
  if (gfx::AngleBetweenVectorsInDegrees(up, world_up) < kSnapThresholdDegrees) {
    snap_to_world_up_ = true;
  }
}

void Repositioner::UpdateTransform(const gfx::Transform& head_pose) {
  gfx::Vector3dF head_up_vector =
      snap_to_world_up_ ? gfx::Vector3dF(0, 1, 0) : vr::GetUpVector(head_pose);
  snap_to_world_up_ = false;

  transform_.ConcatTransform(
      gfx::Transform(gfx::Quaternion(last_laser_direction_, laser_direction_)));

  gfx::Vector3dF new_right_vector = {1, 0, 0};
  transform_.TransformVector(&new_right_vector);

  gfx::Vector3dF new_forward_vector = {0, 0, -1};
  transform_.TransformVector(&new_forward_vector);

  gfx::Vector3dF expected_right_vector =
      gfx::CrossProduct(new_forward_vector, head_up_vector);
  gfx::Quaternion rotate_to_expected_right(new_right_vector,
                                           expected_right_vector);
  transform_.ConcatTransform(gfx::Transform(rotate_to_expected_right));
}

bool Repositioner::OnBeginFrame(const base::TimeTicks& time,
                                const gfx::Transform& head_pose) {
  if (enabled_ || snap_to_world_up_) {
    UpdateTransform(head_pose);
    return true;
  }
  return false;
}

#ifndef NDEBUG
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
#endif

}  // namespace vr
