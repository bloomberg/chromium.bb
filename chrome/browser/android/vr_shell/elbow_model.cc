// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Adapted from:
// https://github.com/googlevr/gvr-unity-sdk/blob/master/Samples/DaydreamLabsControllerPlayground/Assets/GoogleVR/Scripts/Controller/GvrArmModel.cs

#include "chrome/browser/android/vr_shell/elbow_model.h"

#include <cmath>

#include "cc/base/math_util.h"

namespace vr_shell {

namespace {

constexpr gfx::Quaternion kNoRotation = {0.0f, 0.0f, 0.0f, 1.0f};
constexpr gfx::Point3F kDefaultShoulderRight = {0.19f, -0.19f, 0.03f};
constexpr float kFadeDistanceFromFace = 0.34f;
constexpr gfx::Point3F kDefaultRelativeElbow = {0.195f, -0.5f, 0.075f};
constexpr gfx::Point3F kDefaultRelativeWrist = {0.0f, 0.0f, -0.25f};
constexpr gfx::Vector3dF kForward = {0.0f, 0.0f, -1.0f};
constexpr gfx::Vector3dF kUp = {0.0f, 1.0f, 0.0f};
constexpr gfx::Vector3dF kDefaultArmExtensionOffset = {-0.13f, 0.14f, -0.08f};
constexpr float kMinExtensionAngle = 7.0f;
constexpr float kMaxExtenstionAngle = 60.0f;
constexpr float kExtensionWeight = 0.4f;
constexpr float kDeltaAlpha = 3.0f;
constexpr float kDefaultElbowRotationRatio = 0.4f;

gfx::Vector3dF Slerp(const gfx::Vector3dF& a,
                     const gfx::Vector3dF& b,
                     double t) {
  gfx::Vector3dF start;
  gfx::Vector3dF end;
  a.GetNormalized(&start);
  b.GetNormalized(&end);
  float dot =
      cc::MathUtil::ClampToRange(gfx::DotProduct(start, end), -1.0f, 1.0f);
  float theta = acos(dot) * t;
  gfx::Vector3dF relative_vec = end - gfx::ScaleVector3d(start, dot);
  relative_vec.GetNormalized(&relative_vec);
  return gfx::ScaleVector3d(start, cos(theta)) +
         gfx::ScaleVector3d(relative_vec, sin(theta));
}

}  // namespace

ElbowModel::ElbowModel(gvr::ControllerHandedness handedness)
    : handedness_(handedness), alpha_value_(1.0f), torso_direction_(kForward) {
  UpdateHandedness();
}

ElbowModel::~ElbowModel() = default;

void ElbowModel::SetHandedness(gvr::ControllerHandedness handedness) {
  if (handedness == handedness_)
    return;
  handedness_ = handedness;
  UpdateHandedness();
}

void ElbowModel::UpdateHandedness() {
  handed_multiplier_ = {
      handedness_ == GVR_CONTROLLER_RIGHT_HANDED ? 1.0f : -1.0f, 1.0f, 1.0f};
  shoulder_rotation_ = kNoRotation;
  shoulder_position_ =
      gfx::ScalePoint(kDefaultShoulderRight, handed_multiplier_);
}

void ElbowModel::Update(const UpdateData& update) {
  UpdateTorsoDirection(update);
  ApplyArmModel(update);
  UpdateTransparency(update);
}

void ElbowModel::UpdateTorsoDirection(const UpdateData& update) {
  auto head_direction = update.head_direction;
  head_direction.set_y(0);
  head_direction.GetNormalized(&head_direction);

  // Determine the gaze direction horizontally.
  float angular_velocity = update.gyro.Length();
  float gaze_filter_strength =
      cc::MathUtil::ClampToRange((angular_velocity - 0.2f) / 45.0f, 0.0f, 0.1f);
  torso_direction_ =
      Slerp(torso_direction_, head_direction, gaze_filter_strength);

  // Rotate the fixed joints.
  gfx::Quaternion gaze_rotation(kForward, torso_direction_);
  shoulder_rotation_ = gaze_rotation;
  gfx::Vector3dF to_shoulder = shoulder_position_ - gfx::Point3F();
  gfx::Transform(gaze_rotation).TransformVector(&to_shoulder);
  shoulder_position_ = gfx::PointAtOffsetFromOrigin(to_shoulder);
}

void ElbowModel::ApplyArmModel(const UpdateData& update) {
  // Controller's orientation relative to the user.
  auto controller_orientation = update.orientation;
  controller_orientation =
      shoulder_rotation_.inverse() * controller_orientation;

  // Relative positions of the joints.
  elbow_position_ = gfx::ScalePoint(kDefaultRelativeElbow, handed_multiplier_);
  wrist_position_ = gfx::ScalePoint(kDefaultRelativeWrist, handed_multiplier_);
  auto arm_extension_offset =
      gfx::ScaleVector3d(kDefaultArmExtensionOffset, handed_multiplier_);

  // Extract just the x rotation angle.
  gfx::Transform controller_orientation_mat(controller_orientation);

  gfx::Vector3dF controller_forward = kForward;
  controller_orientation_mat.TransformVector(&controller_forward);

  float x_angle =
      90.0f - gfx::AngleBetweenVectorsInDegrees(controller_forward, kUp);

  // Remove the z rotation from the controller
  gfx::Quaternion x_y_rotation(kForward, controller_forward);

  // Offset the elbow by the extension.
  float normalized_angle = (x_angle - kMinExtensionAngle) /
                           (kMaxExtenstionAngle - kMinExtensionAngle);
  float extension_ratio =
      cc::MathUtil::ClampToRange(normalized_angle, 0.0f, 1.0f);
  elbow_position_ = elbow_position_ +
                    gfx::ScaleVector3d(arm_extension_offset, extension_ratio);

  // Calculate the lerp interpolation factor.
  // TODO(vollick): this code incorrectly assumes that the angle is in degrees.
  double total_angle = (kNoRotation * x_y_rotation.inverse()).w();
  float lerp_suppresion = 1.0f - pow(total_angle / 180.0f, 6);
  float lerp_value = lerp_suppresion * (kDefaultElbowRotationRatio +
                                        (1.0f - kDefaultElbowRotationRatio) *
                                            extension_ratio * kExtensionWeight);

  // Apply the absolute rotations to the joints.
  auto lerp_rotation = kNoRotation.Lerp(x_y_rotation, lerp_value);
  elbow_rotation_ =
      (shoulder_rotation_ * lerp_rotation.inverse()) * controller_orientation;
  wrist_rotation_ = shoulder_rotation_ * controller_orientation;

  // Determine the relative positions.
  gfx::Transform(shoulder_rotation_).TransformPoint(&elbow_position_);
  gfx::Vector3dF to_wrist = wrist_position_ - gfx::Point3F();
  gfx::Transform(elbow_rotation_).TransformVector(&to_wrist);
  wrist_position_ = elbow_position_ + to_wrist;
}

void ElbowModel::UpdateTransparency(const UpdateData& update) {
  float distance_to_face = (wrist_position_ - gfx::Point3F()).Length();
  float alpha_change = kDeltaAlpha * update.delta_time_seconds;
  alpha_value_ = cc::MathUtil::ClampToRange(
      distance_to_face < kFadeDistanceFromFace ? alpha_value_ - alpha_change
                                               : alpha_value_ + alpha_change,
      0.0f, 1.0f);
}

}  // namespace vr_shell
