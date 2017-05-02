// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Adapted from:
// https://github.com/googlevr/gvr-unity-sdk/blob/master/Samples/DaydreamLabsControllerPlayground/Assets/GoogleVR/Scripts/Controller/GvrArmModel.cs

#include "chrome/browser/android/vr_shell/elbow_model.h"

#include <cmath>

#include "device/vr/vr_math.h"

namespace vr_shell {

namespace {

constexpr vr::Quatf kNoRotation = {0.0f, 0.0f, 0.0f, 1.0f};
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
      vr::ScalePoint(kDefaultShoulderRight, handed_multiplier_);
}

void ElbowModel::Update(const UpdateData& update) {
  UpdateTorsoDirection(update);
  ApplyArmModel(update);
  UpdateTransparency(update);
}

void ElbowModel::UpdateTorsoDirection(const UpdateData& update) {
  auto head_direction = update.head_direction;
  head_direction.set_y(0);
  vr::NormalizeVector(&head_direction);

  // Determine the gaze direction horizontally.
  float angular_velocity = update.gyro.Length();
  float gaze_filter_strength =
      vr::Clampf((angular_velocity - 0.2f) / 45.0f, 0.0f, 0.1f);
  torso_direction_ =
      vr::QuatSlerp(torso_direction_, head_direction, gaze_filter_strength);

  // Rotate the fixed joints.
  auto gaze_rotation = vr::GetVectorRotation(kForward, torso_direction_);
  shoulder_rotation_ = gaze_rotation;
  vr::Mat4f gaze_rotation_mat;
  vr::QuatToMatrix(gaze_rotation, &gaze_rotation_mat);
  shoulder_position_ = vr::ToPoint(vr::MatrixVectorRotate(
      gaze_rotation_mat, vr::ToVector(shoulder_position_)));
}

void ElbowModel::ApplyArmModel(const UpdateData& update) {
  // Controller's orientation relative to the user.
  auto controller_orientation = update.orientation;
  controller_orientation = vr::QuatProduct(vr::InvertQuat(shoulder_rotation_),
                                           controller_orientation);

  // Relative positions of the joints.
  elbow_position_ = vr::ScalePoint(kDefaultRelativeElbow, handed_multiplier_);
  wrist_position_ = vr::ScalePoint(kDefaultRelativeWrist, handed_multiplier_);
  auto arm_extension_offset =
      vr::ScaleVector(kDefaultArmExtensionOffset, handed_multiplier_);

  // Extract just the x rotation angle.
  vr::Mat4f controller_orientation_mat;
  QuatToMatrix(controller_orientation, &controller_orientation_mat);
  auto controller_forward =
      vr::MatrixVectorRotate(controller_orientation_mat, kForward);
  float x_angle =
      90.0f - gfx::AngleBetweenVectorsInDegrees(controller_forward, kUp);

  // Remove the z rotation from the controller
  auto x_y_rotation = vr::GetVectorRotation(kForward, controller_forward);

  // Offset the elbow by the extension.
  float normalized_angle = (x_angle - kMinExtensionAngle) /
                           (kMaxExtenstionAngle - kMinExtensionAngle);
  float extension_ratio = vr::Clampf(normalized_angle, 0.0f, 1.0f);
  elbow_position_ = elbow_position_ +
                    gfx::ScaleVector3d(arm_extension_offset, extension_ratio);

  // Calculate the lerp interpolation factor.
  float total_angle = vr::QuatAngleDegrees(x_y_rotation, kNoRotation);
  float lerp_suppresion = 1.0f - pow(total_angle / 180.0f, 6);
  float lerp_value = lerp_suppresion * (kDefaultElbowRotationRatio +
                                        (1.0f - kDefaultElbowRotationRatio) *
                                            extension_ratio * kExtensionWeight);

  // Apply the absolute rotations to the joints.
  auto lerp_rotation = vr::QuatLerp(kNoRotation, x_y_rotation, lerp_value);
  elbow_rotation_ = vr::QuatProduct(
      vr::QuatProduct(shoulder_rotation_, vr::InvertQuat(lerp_rotation)),
      controller_orientation);
  wrist_rotation_ = vr::QuatProduct(shoulder_rotation_, controller_orientation);

  // Determine the relative positions.
  vr::Mat4f shoulder_rotation_mat;
  QuatToMatrix(shoulder_rotation_, &shoulder_rotation_mat);
  elbow_position_ = vr::ToPoint(vr::MatrixVectorRotate(
      shoulder_rotation_mat, vr::ToVector(elbow_position_)));
  vr::Mat4f elbow_rotation_mat;
  QuatToMatrix(elbow_rotation_, &elbow_rotation_mat);
  wrist_position_ =
      elbow_position_ +
      vr::MatrixVectorRotate(elbow_rotation_mat, vr::ToVector(wrist_position_));
}

void ElbowModel::UpdateTransparency(const UpdateData& update) {
  float distance_to_face = vr::ToVector(wrist_position_).Length();
  float alpha_change = kDeltaAlpha * update.delta_time_seconds;
  alpha_value_ = vr::Clampf(distance_to_face < kFadeDistanceFromFace
                                ? alpha_value_ - alpha_change
                                : alpha_value_ + alpha_change,
                            0.0f, 1.0f);
}

}  // namespace vr_shell
