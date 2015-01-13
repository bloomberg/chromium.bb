// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/content/display/screen_orientation_controller_chromeos.h"

#include "ash/ash_switches.h"
#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "content/public/browser/screen_orientation_provider.h"
#include "content/public/browser/web_contents.h"
#include "ui/aura/window.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/size.h"

namespace {

// The angle which the screen has to be rotated past before the display will
// rotate to match it (i.e. 45.0f is no stickiness).
const float kDisplayRotationStickyAngleDegrees = 60.0f;

// The minimum acceleration in m/s^2 in a direction required to trigger screen
// rotation. This prevents rapid toggling of rotation when the device is near
// flat and there is very little screen aligned force on it. The value is
// effectively the sine of the rise angle required times the acceleration due
// to gravity, with the current value requiring at least a 25 degree rise.
const float kMinimumAccelerationScreenRotation = 4.2f;

blink::WebScreenOrientationLockType GetDisplayNaturalOrientation() {
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  if (!display_manager->HasInternalDisplay())
    return blink::WebScreenOrientationLockLandscape;

  ash::DisplayInfo info =
      display_manager->GetDisplayInfo(gfx::Display::InternalDisplayId());
  gfx::Size size = info.size_in_pixel();
  switch (info.rotation()) {
    case gfx::Display::ROTATE_0:
    case gfx::Display::ROTATE_180:
      return size.height() >= size.width()
                 ? blink::WebScreenOrientationLockPortrait
                 : blink::WebScreenOrientationLockLandscape;
    case gfx::Display::ROTATE_90:
    case gfx::Display::ROTATE_270:
      return size.height() < size.width()
                 ? blink::WebScreenOrientationLockPortrait
                 : blink::WebScreenOrientationLockLandscape;
  }
  NOTREACHED();
  return blink::WebScreenOrientationLockLandscape;
}

}  // namespace

namespace ash {

ScreenOrientationController::ScreenOrientationController()
    : locking_window_(NULL),
      natural_orientation_(GetDisplayNaturalOrientation()),
      ignore_display_configuration_updates_(false),
      rotation_locked_(false),
      user_rotation_(gfx::Display::ROTATE_0),
      current_rotation_(gfx::Display::ROTATE_0) {
  content::ScreenOrientationProvider::SetDelegate(this);
  Shell::GetInstance()->AddShellObserver(this);
}

ScreenOrientationController::~ScreenOrientationController() {
  content::ScreenOrientationProvider::SetDelegate(NULL);
  Shell::GetInstance()->RemoveShellObserver(this);
  Shell::GetInstance()->accelerometer_reader()->RemoveObserver(this);
  Shell::GetInstance()->display_controller()->RemoveObserver(this);
}

void ScreenOrientationController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ScreenOrientationController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ScreenOrientationController::SetRotationLocked(bool rotation_locked) {
  if (rotation_locked_ == rotation_locked)
    return;
  rotation_locked_ = rotation_locked;
  FOR_EACH_OBSERVER(Observer, observers_,
                    OnRotationLockChanged(rotation_locked_));
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  if (!display_manager->HasInternalDisplay())
    return;
  base::AutoReset<bool> auto_ignore_display_configuration_updates(
      &ignore_display_configuration_updates_, true);
  display_manager->RegisterDisplayRotationProperties(rotation_locked_,
                                                     current_rotation_);
}

void ScreenOrientationController::SetDisplayRotation(
    gfx::Display::Rotation rotation) {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  if (!display_manager->HasInternalDisplay())
    return;
  current_rotation_ = rotation;
  base::AutoReset<bool> auto_ignore_display_configuration_updates(
      &ignore_display_configuration_updates_, true);
  display_manager->SetDisplayRotation(gfx::Display::InternalDisplayId(),
                                      rotation);
}

void ScreenOrientationController::OnAccelerometerUpdated(
    const ui::AccelerometerUpdate& update) {
  if (rotation_locked_)
    return;
  if (!update.has(ui::ACCELEROMETER_SOURCE_SCREEN))
    return;
  // Ignore the reading if it appears unstable. The reading is considered
  // unstable if it deviates too much from gravity
  if (chromeos::AccelerometerReader::IsReadingStable(
          update, ui::ACCELEROMETER_SOURCE_SCREEN)) {
    HandleScreenRotation(update.get(ui::ACCELEROMETER_SOURCE_SCREEN));
  }
}

bool ScreenOrientationController::FullScreenRequired(
    content::WebContents* web_contents) {
  return true;
}

void ScreenOrientationController::Lock(
    content::WebContents* web_contents,
    blink::WebScreenOrientationLockType lock_orientation) {
  aura::Window* requesting_window = web_contents->GetNativeView();
  // TODO(jonross): Track one rotation lock per window. When the active window
  // changes apply any corresponding rotation lock.
  if (!locking_window_)
    locking_window_ = requesting_window;
  else if (requesting_window != locking_window_)
    return;

  switch (lock_orientation) {
    case blink::WebScreenOrientationLockAny:
      SetRotationLocked(false);
      locking_window_ = NULL;
      break;
    case blink::WebScreenOrientationLockDefault:
      NOTREACHED();
      break;
    case blink::WebScreenOrientationLockPortraitPrimary:
      LockRotationToPrimaryOrientation(blink::WebScreenOrientationLockPortrait);
      break;
    case blink::WebScreenOrientationLockLandscape:
    case blink::WebScreenOrientationLockPortrait:
      LockToRotationMatchingOrientation(lock_orientation);
      break;
    case blink::WebScreenOrientationLockPortraitSecondary:
      LockRotationToSecondaryOrientation(
          blink::WebScreenOrientationLockPortrait);
      break;
    case blink::WebScreenOrientationLockLandscapeSecondary:
      LockRotationToSecondaryOrientation(
          blink::WebScreenOrientationLockLandscape);
      break;
    case blink::WebScreenOrientationLockLandscapePrimary:
      LockRotationToPrimaryOrientation(
          blink::WebScreenOrientationLockLandscape);
      break;
    case blink::WebScreenOrientationLockNatural:
      LockRotation(gfx::Display::ROTATE_0);
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool ScreenOrientationController::ScreenOrientationProviderSupported() {
  return Shell::GetInstance()
             ->maximize_mode_controller()
             ->IsMaximizeModeWindowManagerEnabled() &&
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kAshEnableTouchViewTesting);
}

void ScreenOrientationController::Unlock(content::WebContents* web_contents) {
  aura::Window* requesting_window = web_contents->GetNativeView();
  if (requesting_window != locking_window_)
    return;
  locking_window_ = NULL;
  SetRotationLocked(false);
}

void ScreenOrientationController::OnDisplayConfigurationChanged() {
  if (ignore_display_configuration_updates_)
    return;
  gfx::Display::Rotation user_rotation =
      Shell::GetInstance()
          ->display_manager()
          ->GetDisplayInfo(gfx::Display::InternalDisplayId())
          .rotation();
  if (user_rotation != current_rotation_) {
    // A user may change other display configuration settings. When the user
    // does change the rotation setting, then lock rotation to prevent the
    // accelerometer from erasing their change.
    SetRotationLocked(true);
    user_rotation_ = current_rotation_ = user_rotation;
  }
}

void ScreenOrientationController::OnMaximizeModeStarted() {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  if (!display_manager->HasInternalDisplay())
    return;
  current_rotation_ = user_rotation_ =
      display_manager->GetDisplayInfo(gfx::Display::InternalDisplayId())
          .rotation();
  LoadDisplayRotationProperties();
  Shell::GetInstance()->accelerometer_reader()->AddObserver(this);
  Shell::GetInstance()->display_controller()->AddObserver(this);
}

void ScreenOrientationController::OnMaximizeModeEnded() {
  if (!Shell::GetInstance()->display_manager()->HasInternalDisplay())
    return;
  Shell::GetInstance()->accelerometer_reader()->RemoveObserver(this);
  Shell::GetInstance()->display_controller()->RemoveObserver(this);
  if (current_rotation_ != user_rotation_)
    SetDisplayRotation(user_rotation_);
}

void ScreenOrientationController::LockRotation(
    gfx::Display::Rotation rotation) {
  SetRotationLocked(true);
  SetDisplayRotation(rotation);
}

void ScreenOrientationController::LockRotationToPrimaryOrientation(
    blink::WebScreenOrientationLockType lock_orientation) {
  LockRotation(natural_orientation_ == lock_orientation
                   ? gfx::Display::ROTATE_0
                   : gfx::Display::ROTATE_90);
}

void ScreenOrientationController::LockRotationToSecondaryOrientation(
    blink::WebScreenOrientationLockType lock_orientation) {
  LockRotation(natural_orientation_ == lock_orientation
                   ? gfx::Display::ROTATE_180
                   : gfx::Display::ROTATE_270);
}

void ScreenOrientationController::LockToRotationMatchingOrientation(
    blink::WebScreenOrientationLockType lock_orientation) {
  // TODO(jonross): Update MaximizeModeController to allow rotation between
  // two angles of an orientation (e.g. from ROTATE_0 to ROTATE_180, and from
  // ROTATE_90 to ROTATE_270)
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  if (!display_manager->HasInternalDisplay())
    return;

  gfx::Display::Rotation rotation =
      display_manager->GetDisplayInfo(gfx::Display::InternalDisplayId())
          .rotation();
  if (natural_orientation_ == lock_orientation) {
    if (rotation == gfx::Display::ROTATE_0 ||
        rotation == gfx::Display::ROTATE_180) {
      SetRotationLocked(true);
    } else {
      LockRotation(gfx::Display::ROTATE_0);
    }
  } else {
    if (rotation == gfx::Display::ROTATE_90 ||
        rotation == gfx::Display::ROTATE_270) {
      SetRotationLocked(true);
    } else {
      LockRotation(gfx::Display::ROTATE_90);
    }
  }
}

void ScreenOrientationController::HandleScreenRotation(
    const gfx::Vector3dF& lid) {
  gfx::Vector3dF lid_flattened(lid.x(), lid.y(), 0.0f);
  float lid_flattened_length = lid_flattened.Length();
  // When the lid is close to being flat, don't change rotation as it is too
  // sensitive to slight movements.
  if (lid_flattened_length < kMinimumAccelerationScreenRotation)
    return;

  // The reference vector is the angle of gravity when the device is rotated
  // clockwise by 45 degrees. Computing the angle between this vector and
  // gravity we can easily determine the expected display rotation.
  static const gfx::Vector3dF rotation_reference(-1.0f, -1.0f, 0.0f);

  // Set the down vector to match the expected direction of gravity given the
  // last configured rotation. This is used to enforce a stickiness that the
  // user must overcome to rotate the display and prevents frequent rotations
  // when holding the device near 45 degrees.
  gfx::Vector3dF down(0.0f, 0.0f, 0.0f);
  if (current_rotation_ == gfx::Display::ROTATE_0)
    down.set_y(-1.0f);
  else if (current_rotation_ == gfx::Display::ROTATE_90)
    down.set_x(-1.0f);
  else if (current_rotation_ == gfx::Display::ROTATE_180)
    down.set_y(1.0f);
  else
    down.set_x(1.0f);

  // Don't rotate if the screen has not passed the threshold.
  if (gfx::AngleBetweenVectorsInDegrees(down, lid_flattened) <
      kDisplayRotationStickyAngleDegrees) {
    return;
  }

  float angle = gfx::ClockwiseAngleBetweenVectorsInDegrees(
      rotation_reference, lid_flattened, gfx::Vector3dF(0.0f, 0.0f, -1.0f));

  gfx::Display::Rotation new_rotation = gfx::Display::ROTATE_90;
  if (angle < 90.0f)
    new_rotation = gfx::Display::ROTATE_0;
  else if (angle < 180.0f)
    new_rotation = gfx::Display::ROTATE_270;
  else if (angle < 270.0f)
    new_rotation = gfx::Display::ROTATE_180;

  if (new_rotation != current_rotation_)
    SetDisplayRotation(new_rotation);
}

void ScreenOrientationController::LoadDisplayRotationProperties() {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  if (!display_manager->registered_internal_display_rotation_lock())
    return;
  SetDisplayRotation(display_manager->registered_internal_display_rotation());
  SetRotationLocked(true);
}

}  // namespace ash
