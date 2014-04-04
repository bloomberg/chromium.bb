// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/maximize_mode_controller.h"

#include "ash/accelerometer/accelerometer_controller.h"
#include "ash/shell.h"
#include "ui/gfx/vector3d_f.h"

namespace ash {

namespace {

// The hinge angle at which to enter maximize mode.
const float kEnterMaximizeModeAngle = 200.0f;

// The angle at which to exit maximize mode, this is specifically less than the
// angle to enter maximize mode to prevent rapid toggling when near the angle.
const float kExitMaximizeModeAngle = 160.0f;

// When the lid is fully open 360 degrees, the accelerometer readings can
// occasionally appear as though the lid is almost closed. If the lid appears
// near closed but the device is on we assume it is an erroneous reading from
// it being open 360 degrees.
const float kFullyOpenAngleErrorTolerance = 10.0f;

// When the device approaches vertical orientation (i.e. portrait orientation)
// the accelerometers for the base and lid approach the same values (i.e.
// gravity pointing in the direction of the hinge). When this happens we cannot
// compute the hinge angle reliably and must turn ignore accelerometer readings.
// This is the angle from vertical under which we will not compute a hinge
// angle.
const float kHingeAxisAlignedThreshold = 15.0f;

const float kRadiansToDegrees = 180.0f / 3.14159265f;

// Returns the angle between |base| and |other| in degrees.
float AngleBetweenVectorsInDegrees(const gfx::Vector3dF& base,
                                 const gfx::Vector3dF& other) {
  return acos(gfx::DotProduct(base, other) /
              base.Length() / other.Length()) * kRadiansToDegrees;
}

// Returns the clockwise angle between |base| and |other| where |normal| is the
// normal of the virtual surface to measure clockwise according to.
float ClockwiseAngleBetweenVectorsInDegrees(const gfx::Vector3dF& base,
                                            const gfx::Vector3dF& other,
                                            const gfx::Vector3dF& normal) {
  float angle = AngleBetweenVectorsInDegrees(base, other);
  gfx::Vector3dF cross(base);
  cross.Cross(other);
  // If the dot product of this cross product is normal, it means that the
  // shortest angle between |base| and |other| was counterclockwise with respect
  // to the surface represented by |normal| and this angle must be reversed.
  if (gfx::DotProduct(cross, normal) > 0.0f)
    angle = 360.0f - angle;
  return angle;
}

}  // namespace

MaximizeModeController::MaximizeModeController() {
  Shell::GetInstance()->accelerometer_controller()->AddObserver(this);
}

MaximizeModeController::~MaximizeModeController() {
  Shell::GetInstance()->accelerometer_controller()->RemoveObserver(this);
}

void MaximizeModeController::OnAccelerometerUpdated(
    const gfx::Vector3dF& base,
    const gfx::Vector3dF& lid) {
  static const gfx::Vector3dF hinge_vector(0.0f, 1.0f, 0.0f);

  // As the hinge approaches a vertical angle, the base and lid accelerometers
  // approach the same values making any angle calculations highly inaccurate.
  // Bail out early when it is too close.
  float hinge_angle = AngleBetweenVectorsInDegrees(base, hinge_vector);
  if (hinge_angle < kHingeAxisAlignedThreshold ||
      hinge_angle > 180.0f - kHingeAxisAlignedThreshold)
    return;

  // Compute the angle between the base and the lid.
  float angle = ClockwiseAngleBetweenVectorsInDegrees(base, lid, hinge_vector);

  // Toggle maximize mode on or off when corresponding thresholds are passed.
  // TODO(flackr): Make MaximizeModeController own the MaximizeModeWindowManager
  // such that observations of state changes occur after the change and shell
  // has fewer states to track.
  bool maximize_mode_engaged =
      Shell::GetInstance()->IsMaximizeModeWindowManagerEnabled();
  if (maximize_mode_engaged &&
      angle > kFullyOpenAngleErrorTolerance &&
      angle < kExitMaximizeModeAngle) {
    Shell::GetInstance()->EnableMaximizeModeWindowManager(false);
  } else if (!maximize_mode_engaged &&
      angle > kEnterMaximizeModeAngle) {
    Shell::GetInstance()->EnableMaximizeModeWindowManager(true);
  }
}

}  // namespace ash
