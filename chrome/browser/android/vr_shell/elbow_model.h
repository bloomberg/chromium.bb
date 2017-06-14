// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Adapted from:
// https://github.com/googlevr/gvr-unity-sdk/blob/master/Samples/DaydreamLabsControllerPlayground/Assets/GoogleVR/Scripts/Controller/GvrArmModel.cs

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_ELBOW_MODEL_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_ELBOW_MODEL_H_

#include "third_party/gvr-android-sdk/src/libraries/headers/vr/gvr/capi/include/gvr_types.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform.h"

namespace vr_shell {

class ElbowModel {
 public:
  struct UpdateData {
    bool connected;
    gfx::Quaternion orientation;
    gfx::Vector3dF gyro;
    gfx::Vector3dF head_direction;
    float delta_time_seconds;
  };

  explicit ElbowModel(gvr::ControllerHandedness handedness);
  ~ElbowModel();

  const gfx::Point3F& GetControllerPosition() const { return wrist_position_; }
  const gfx::Quaternion& GetControllerRotation() const {
    return wrist_rotation_;
  }
  float GetAlphaValue() const { return alpha_value_; }

  void Update(const UpdateData& update);
  void SetHandedness(gvr::ControllerHandedness handedness);

 private:
  void UpdateHandedness();
  void UpdateTorsoDirection(const UpdateData& update);
  void ApplyArmModel(const UpdateData& update);
  void UpdateTransparency(const UpdateData& update);

  gvr::ControllerHandedness handedness_;

  gfx::Point3F wrist_position_;
  gfx::Quaternion wrist_rotation_;
  float alpha_value_;

  gfx::Point3F elbow_position_;
  gfx::Quaternion elbow_rotation_;
  gfx::Point3F shoulder_position_;
  gfx::Quaternion shoulder_rotation_;
  gfx::Vector3dF torso_direction_;
  gfx::Vector3dF handed_multiplier_;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_ELBOW_MODEL_H_
