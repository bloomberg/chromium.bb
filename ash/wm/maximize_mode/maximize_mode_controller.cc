// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/maximize_mode_controller.h"

#include "ash/accelerometer/accelerometer_controller.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/wm/maximize_mode/maximize_mode_event_blocker.h"
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
const float kFullyOpenAngleErrorTolerance = 20.0f;

// When the device approaches vertical orientation (i.e. portrait orientation)
// the accelerometers for the base and lid approach the same values (i.e.
// gravity pointing in the direction of the hinge). When this happens we cannot
// compute the hinge angle reliably and must turn ignore accelerometer readings.
// This is the angle from vertical under which we will not compute a hinge
// angle.
const float kHingeAxisAlignedThreshold = 15.0f;

// The maximum deviation from the acceleration expected due to gravity under
// which to detect hinge angle and screen rotation.
const float kDeviationFromGravityThreshold = 0.1f;

// The maximum deviation between the magnitude of the two accelerometers under
// which to detect hinge angle and screen rotation. These accelerometers are
// attached to the same physical device and so should be under the same
// acceleration.
const float kNoisyMagnitudeDeviation = 0.1f;

// The angle which the screen has to be rotated past before the display will
// rotate to match it (i.e. 45.0f is no stickiness).
const float kDisplayRotationStickyAngleDegrees = 60.0f;

// The minimum acceleration in a direction required to trigger screen rotation.
// This prevents rapid toggling of rotation when the device is near flat and
// there is very little screen aligned force on it.
const float kMinimumAccelerationScreenRotation = 0.3f;

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

MaximizeModeController::MaximizeModeController()
    : rotation_locked_(false) {
  Shell::GetInstance()->accelerometer_controller()->AddObserver(this);
}

MaximizeModeController::~MaximizeModeController() {
  Shell::GetInstance()->accelerometer_controller()->RemoveObserver(this);
}

void MaximizeModeController::OnAccelerometerUpdated(
    const gfx::Vector3dF& base,
    const gfx::Vector3dF& lid) {
  // Ignore the reading if it appears unstable. The reading is considered
  // unstable if it deviates too much from gravity and/or the magnitude of the
  // reading from the lid differs too much from the reading from the base.
  float base_magnitude = base.Length();
  float lid_magnitude = lid.Length();
  if (std::abs(base_magnitude - lid_magnitude) > kNoisyMagnitudeDeviation ||
      std::abs(base_magnitude - 1.0f) > kDeviationFromGravityThreshold ||
      std::abs(lid_magnitude - 1.0f) > kDeviationFromGravityThreshold) {
      return;
  }

  // Responding to the hinge rotation can change the maximize mode state which
  // affects screen rotation, so we handle hinge rotation first.
  HandleHingeRotation(base, lid);
  HandleScreenRotation(lid);
}

void MaximizeModeController::HandleHingeRotation(const gfx::Vector3dF& base,
                                                 const gfx::Vector3dF& lid) {
  static const gfx::Vector3dF hinge_vector(0.0f, 1.0f, 0.0f);
  bool maximize_mode_engaged =
      Shell::GetInstance()->IsMaximizeModeWindowManagerEnabled();

  // As the hinge approaches a vertical angle, the base and lid accelerometers
  // approach the same values making any angle calculations highly inaccurate.
  // Bail out early when it is too close.
  float hinge_angle = AngleBetweenVectorsInDegrees(base, hinge_vector);
  if (hinge_angle < kHingeAxisAlignedThreshold ||
      hinge_angle > 180.0f - kHingeAxisAlignedThreshold) {
    return;
  }

  // Compute the angle between the base and the lid.
  float angle = ClockwiseAngleBetweenVectorsInDegrees(base, lid, hinge_vector);

  // Toggle maximize mode on or off when corresponding thresholds are passed.
  // TODO(flackr): Make MaximizeModeController own the MaximizeModeWindowManager
  // such that observations of state changes occur after the change and shell
  // has fewer states to track.
  if (maximize_mode_engaged &&
      angle > kFullyOpenAngleErrorTolerance &&
      angle < kExitMaximizeModeAngle) {
    Shell::GetInstance()->EnableMaximizeModeWindowManager(false);
    event_blocker_.reset();
  } else if (!maximize_mode_engaged &&
      angle > kEnterMaximizeModeAngle) {
    Shell::GetInstance()->EnableMaximizeModeWindowManager(true);
    event_blocker_.reset(new MaximizeModeEventBlocker);
  }
}

void MaximizeModeController::HandleScreenRotation(const gfx::Vector3dF& lid) {
  bool maximize_mode_engaged =
      Shell::GetInstance()->IsMaximizeModeWindowManagerEnabled();

  DisplayManager* display_manager =
      Shell::GetInstance()->display_manager();
  gfx::Display::Rotation current_rotation = display_manager->GetDisplayInfo(
      gfx::Display::InternalDisplayId()).rotation();

  // If maximize mode is not engaged, ensure the screen is not rotated and
  // do not rotate to match the current device orientation.
  if (!maximize_mode_engaged) {
    if (current_rotation != gfx::Display::ROTATE_0) {
      // TODO(flackr): Currently this will prevent setting a manual rotation on
      // the screen of a device with an accelerometer, this should only set it
      // back to ROTATE_0 if it was last set by the accelerometer.
      // Also, SetDisplayRotation will save the setting to the local store,
      // this should be stored in a way that we can distinguish what the
      // rotation was set by.
      display_manager->SetDisplayRotation(gfx::Display::InternalDisplayId(),
                                          gfx::Display::ROTATE_0);
    }
    rotation_locked_ = false;
    return;
  }

  if (rotation_locked_)
    return;

  // After determining maximize mode state, determine if the screen should
  // be rotated.
  gfx::Vector3dF lid_flattened(lid.x(), lid.y(), 0.0f);
  float lid_flattened_length = lid_flattened.Length();
  // When the lid is close to being flat, don't change rotation as it is too
  // sensitive to slight movements.
  if (lid_flattened_length < kMinimumAccelerationScreenRotation)
    return;

  // The reference vector is the angle of gravity when the device is rotated
  // clockwise by 45 degrees. Computing the angle between this vector and
  // gravity we can easily determine the expected display rotation.
  static gfx::Vector3dF rotation_reference(-1.0f, 1.0f, 0.0f);

  // Set the down vector to match the expected direction of gravity given the
  // last configured rotation. This is used to enforce a stickiness that the
  // user must overcome to rotate the display and prevents frequent rotations
  // when holding the device near 45 degrees.
  gfx::Vector3dF down(0.0f, 0.0f, 0.0f);
  if (current_rotation == gfx::Display::ROTATE_0)
    down.set_x(-1.0f);
  else if (current_rotation == gfx::Display::ROTATE_90)
    down.set_y(1.0f);
  else if (current_rotation == gfx::Display::ROTATE_180)
    down.set_x(1.0f);
  else
    down.set_y(-1.0f);

  // Don't rotate if the screen has not passed the threshold.
  if (AngleBetweenVectorsInDegrees(down, lid_flattened) <
      kDisplayRotationStickyAngleDegrees) {
    return;
  }

  float angle = ClockwiseAngleBetweenVectorsInDegrees(rotation_reference,
      lid_flattened, gfx::Vector3dF(0.0f, 0.0f, -1.0f));

  gfx::Display::Rotation new_rotation = gfx::Display::ROTATE_90;
  if (angle < 90.0f)
    new_rotation = gfx::Display::ROTATE_0;
  else if (angle < 180.0f)
    new_rotation = gfx::Display::ROTATE_270;
  else if (angle < 270.0f)
    new_rotation = gfx::Display::ROTATE_180;

  // When exiting maximize mode return rotation to 0. When entering, rotate to
  // match screen orientation.
  if (new_rotation == gfx::Display::ROTATE_0 ||
      maximize_mode_engaged) {
    display_manager->SetDisplayRotation(gfx::Display::InternalDisplayId(),
                                        new_rotation);
  }
}

}  // namespace ash
