// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/screen_orientation_controller_chromeos.h"

#include "ash/common/ash_switches.h"
#include "ash/common/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/shared/app_types.h"
#include "ash/shell.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "ui/chromeos/accelerometer/accelerometer_util.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/gfx/geometry/size.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

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
  if (!display::Display::HasInternalDisplay())
    return blink::WebScreenOrientationLockLandscape;

  display::ManagedDisplayInfo info =
      WmShell::Get()->GetDisplayInfo(display::Display::InternalDisplayId());
  gfx::Size size = info.size_in_pixel();
  switch (info.GetActiveRotation()) {
    case display::Display::ROTATE_0:
    case display::Display::ROTATE_180:
      return size.height() >= size.width()
                 ? blink::WebScreenOrientationLockPortrait
                 : blink::WebScreenOrientationLockLandscape;
    case display::Display::ROTATE_90:
    case display::Display::ROTATE_270:
      return size.height() < size.width()
                 ? blink::WebScreenOrientationLockPortrait
                 : blink::WebScreenOrientationLockLandscape;
  }
  NOTREACHED();
  return blink::WebScreenOrientationLockLandscape;
}

blink::WebScreenOrientationLockType RotationToOrientation(
    display::Display::Rotation rotation) {
  blink::WebScreenOrientationLockType natural = GetDisplayNaturalOrientation();
  if (natural == blink::WebScreenOrientationLockLandscape) {
    switch (rotation) {
      case display::Display::ROTATE_0:
        return blink::WebScreenOrientationLockLandscapePrimary;
      case display::Display::ROTATE_90:
        return blink::WebScreenOrientationLockPortraitPrimary;
      case display::Display::ROTATE_180:
        return blink::WebScreenOrientationLockLandscapeSecondary;
      case display::Display::ROTATE_270:
        return blink::WebScreenOrientationLockPortraitSecondary;
    }
  } else {  // Natural portrait
    switch (rotation) {
      case display::Display::ROTATE_0:
        return blink::WebScreenOrientationLockPortraitPrimary;
      case display::Display::ROTATE_90:
        return blink::WebScreenOrientationLockLandscapePrimary;
      case display::Display::ROTATE_180:
        return blink::WebScreenOrientationLockPortraitSecondary;
      case display::Display::ROTATE_270:
        return blink::WebScreenOrientationLockLandscapeSecondary;
    }
  }
  NOTREACHED();
  return blink::WebScreenOrientationLockAny;
}

// Returns the rotation that matches the orientation type.
// Returns ROTATE_0 if the given orientation is ANY, which is used
// to indicate that user didn't lock orientation.
display::Display::Rotation OrientationToRotation(
    blink::WebScreenOrientationLockType orientation) {
  blink::WebScreenOrientationLockType natural = GetDisplayNaturalOrientation();
  if (orientation == blink::WebScreenOrientationLockAny) {
    return display::Display::ROTATE_0;
  }

  if (natural == blink::WebScreenOrientationLockLandscape) {
    switch (orientation) {
      case blink::WebScreenOrientationLockLandscapePrimary:
        return display::Display::ROTATE_0;
      case blink::WebScreenOrientationLockPortraitPrimary:
        return display::Display::ROTATE_90;
      case blink::WebScreenOrientationLockLandscapeSecondary:
        return display::Display::ROTATE_180;
      case blink::WebScreenOrientationLockPortraitSecondary:
        return display::Display::ROTATE_270;
      default:
        break;
    }
  } else {  // Natural portrait
    switch (orientation) {
      case blink::WebScreenOrientationLockPortraitPrimary:
        return display::Display::ROTATE_0;
      case blink::WebScreenOrientationLockLandscapePrimary:
        return display::Display::ROTATE_90;
      case blink::WebScreenOrientationLockPortraitSecondary:
        return display::Display::ROTATE_180;
      case blink::WebScreenOrientationLockLandscapeSecondary:
        return display::Display::ROTATE_270;
      default:
        break;
    }
  }
  NOTREACHED() << orientation;
  return display::Display::ROTATE_0;
}

// Returns the locked orientation that matches the application
// requested orientation, or the application orientation itself
// if it didn't match.
blink::WebScreenOrientationLockType ResolveOrientationLock(
    blink::WebScreenOrientationLockType app_requested,
    blink::WebScreenOrientationLockType lock) {
  if (app_requested == blink::WebScreenOrientationLockAny ||
      (app_requested == blink::WebScreenOrientationLockLandscape &&
       (lock == blink::WebScreenOrientationLockLandscapePrimary ||
        lock == blink::WebScreenOrientationLockLandscapeSecondary)) ||
      (app_requested == blink::WebScreenOrientationLockPortrait &&
       (lock == blink::WebScreenOrientationLockPortraitPrimary ||
        lock == blink::WebScreenOrientationLockPortraitSecondary))) {
    return lock;
  }
  return app_requested;
}

}  // namespace

ScreenOrientationController::ScreenOrientationController()
    : natural_orientation_(GetDisplayNaturalOrientation()),
      ignore_display_configuration_updates_(false),
      rotation_locked_(false),
      rotation_locked_orientation_(blink::WebScreenOrientationLockAny),
      user_rotation_(display::Display::ROTATE_0),
      current_rotation_(display::Display::ROTATE_0) {
  Shell::GetInstance()->AddShellObserver(this);
}

ScreenOrientationController::~ScreenOrientationController() {
  Shell::GetInstance()->RemoveShellObserver(this);
  chromeos::AccelerometerReader::GetInstance()->RemoveObserver(this);
  WmShell::Get()->RemoveDisplayObserver(this);
  Shell::GetInstance()->activation_client()->RemoveObserver(this);
  for (auto& windows : locking_windows_)
    windows.first->aura_window()->RemoveObserver(this);
}

void ScreenOrientationController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ScreenOrientationController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ScreenOrientationController::LockOrientationForWindow(
    WmWindow* requesting_window,
    blink::WebScreenOrientationLockType lock_orientation) {
  if (locking_windows_.empty())
    Shell::GetInstance()->activation_client()->AddObserver(this);

  if (!requesting_window->aura_window()->HasObserver(this))
    requesting_window->aura_window()->AddObserver(this);
  locking_windows_[requesting_window] = lock_orientation;

  ApplyLockForActiveWindow();
}

void ScreenOrientationController::UnlockOrientationForWindow(WmWindow* window) {
  locking_windows_.erase(window);
  if (locking_windows_.empty())
    Shell::GetInstance()->activation_client()->RemoveObserver(this);
  window->aura_window()->RemoveObserver(this);
  ApplyLockForActiveWindow();
}

void ScreenOrientationController::UnlockAll() {
  for (auto pair : locking_windows_)
    pair.first->aura_window()->RemoveObserver(this);
  locking_windows_.clear();
  Shell::GetInstance()->activation_client()->RemoveObserver(this);
  SetRotationLockedInternal(false);
  if (user_rotation_ != current_rotation_)
    SetDisplayRotation(user_rotation_, display::Display::ROTATION_SOURCE_USER);
}

bool ScreenOrientationController::ScreenOrientationProviderSupported() const {
  return Shell::Get()
             ->maximize_mode_controller()
             ->IsMaximizeModeWindowManagerEnabled() &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kAshDisableScreenOrientationLock);
}

void ScreenOrientationController::ToggleUserRotationLock() {
  if (!display::Display::HasInternalDisplay())
    return;

  if (user_rotation_locked()) {
    user_locked_orientation_ = blink::WebScreenOrientationLockAny;
  } else {
    display::Display::Rotation current_rotation =
        WmShell::Get()
            ->GetDisplayInfo(display::Display::InternalDisplayId())
            .GetActiveRotation();
    user_locked_orientation_ = RotationToOrientation(current_rotation);
  }
  base::AutoReset<bool> auto_ignore_display_configuration_updates(
      &ignore_display_configuration_updates_, true);
  Shell::GetInstance()->display_manager()->RegisterDisplayRotationProperties(
      user_rotation_locked(), OrientationToRotation(user_locked_orientation_));

  ApplyLockForActiveWindow();
  for (auto& observer : observers_)
    observer.OnUserRotationLockChanged();
}

void ScreenOrientationController::OnWindowActivated(ActivationReason reason,
                                                    aura::Window* gained_active,
                                                    aura::Window* lost_active) {
  ApplyLockForActiveWindow();
}

void ScreenOrientationController::OnWindowDestroying(aura::Window* window) {
  UnlockOrientationForWindow(WmWindow::Get(window));
}

// Currently contents::WebContents will only be able to lock rotation while
// fullscreen. In this state a user cannot click on the tab strip to change. If
// this becomes supported for non-fullscreen tabs then the following interferes
// with TabDragController. OnWindowVisibilityChanged is called between a mouse
// down and mouse up. The rotation this triggers leads to a coordinate space
// change in the middle of an event. Causes the tab to separate from the tab
// strip.
void ScreenOrientationController::OnWindowVisibilityChanged(
    aura::Window* window,
    bool visible) {
  if (locking_windows_.find(WmWindow::Get(window)) == locking_windows_.end())
    return;
  ApplyLockForActiveWindow();
}

void ScreenOrientationController::OnAccelerometerUpdated(
    scoped_refptr<const chromeos::AccelerometerUpdate> update) {
  if (rotation_locked_ && !CanRotateInLockedState())
    return;
  if (!update->has(chromeos::ACCELEROMETER_SOURCE_SCREEN))
    return;
  // Ignore the reading if it appears unstable. The reading is considered
  // unstable if it deviates too much from gravity
  if (ui::IsAccelerometerReadingStable(*update,
                                       chromeos::ACCELEROMETER_SOURCE_SCREEN)) {
    HandleScreenRotation(update->get(chromeos::ACCELEROMETER_SOURCE_SCREEN));
  }
}

void ScreenOrientationController::OnDisplayConfigurationChanged() {
  if (ignore_display_configuration_updates_)
    return;
  if (!display::Display::HasInternalDisplay())
    return;
  display::Display::Rotation user_rotation =
      WmShell::Get()
          ->GetDisplayInfo(display::Display::InternalDisplayId())
          .GetActiveRotation();
  if (user_rotation != current_rotation_) {
    // TODO(oshima): We should disable the orientation change in settings
    // because application may not work correctly.

    // A user may change other display configuration settings. When the user
    // does change the rotation setting, then lock rotation to prevent the
    // accelerometer from erasing their change.
    SetRotationLockedInternal(true);
    user_rotation_ = current_rotation_ = user_rotation;
  }
}

void ScreenOrientationController::OnMaximizeModeStarted() {
  // Do not exit early, as the internal display can be determined after Maximize
  // Mode has started. (chrome-os-partner:38796)
  // Always start observing.
  if (display::Display::HasInternalDisplay()) {
    current_rotation_ = user_rotation_ =
        WmShell::Get()
            ->GetDisplayInfo(display::Display::InternalDisplayId())
            .GetActiveRotation();
  }
  if (!rotation_locked_)
    LoadDisplayRotationProperties();
  chromeos::AccelerometerReader::GetInstance()->AddObserver(this);
  WmShell::Get()->AddDisplayObserver(this);

  if (!display::Display::HasInternalDisplay())
    return;
  ApplyLockForActiveWindow();
  for (auto& observer : observers_)
    observer.OnUserRotationLockChanged();
}

void ScreenOrientationController::OnMaximizeModeEnded() {
  chromeos::AccelerometerReader::GetInstance()->RemoveObserver(this);
  WmShell::Get()->RemoveDisplayObserver(this);
  if (!display::Display::HasInternalDisplay())
    return;
  if (current_rotation_ != user_rotation_)
    SetDisplayRotation(user_rotation_, display::Display::ROTATION_SOURCE_USER);
  for (auto& observer : observers_)
    observer.OnUserRotationLockChanged();
}

void ScreenOrientationController::SetDisplayRotation(
    display::Display::Rotation rotation,
    display::Display::RotationSource source) {
  if (!display::Display::HasInternalDisplay())
    return;
  current_rotation_ = rotation;
  base::AutoReset<bool> auto_ignore_display_configuration_updates(
      &ignore_display_configuration_updates_, true);

  Shell::GetInstance()->display_configuration_controller()->SetDisplayRotation(
      display::Display::InternalDisplayId(), rotation, source);
}

void ScreenOrientationController::SetRotationLockedInternal(
    bool rotation_locked) {
  if (rotation_locked_ == rotation_locked)
    return;
  rotation_locked_ = rotation_locked;
  if (!rotation_locked_)
    rotation_locked_orientation_ = blink::WebScreenOrientationLockAny;
}

void ScreenOrientationController::LockRotation(
    display::Display::Rotation rotation,
    display::Display::RotationSource source) {
  SetRotationLockedInternal(true);
  SetDisplayRotation(rotation, source);
}

void ScreenOrientationController::LockRotationToOrientation(
    blink::WebScreenOrientationLockType lock_orientation) {
  rotation_locked_orientation_ = lock_orientation;
  switch (lock_orientation) {
    case blink::WebScreenOrientationLockAny:
      SetRotationLockedInternal(false);
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
      LockRotation(display::Display::ROTATE_0,
                   display::Display::ROTATION_SOURCE_ACTIVE);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ScreenOrientationController::LockRotationToPrimaryOrientation(
    blink::WebScreenOrientationLockType lock_orientation) {
  LockRotation(natural_orientation_ == lock_orientation
                   ? display::Display::ROTATE_0
                   : display::Display::ROTATE_90,
               display::Display::ROTATION_SOURCE_ACTIVE);
}

void ScreenOrientationController::LockRotationToSecondaryOrientation(
    blink::WebScreenOrientationLockType lock_orientation) {
  LockRotation(natural_orientation_ == lock_orientation
                   ? display::Display::ROTATE_180
                   : display::Display::ROTATE_270,
               display::Display::ROTATION_SOURCE_ACTIVE);
}

void ScreenOrientationController::LockToRotationMatchingOrientation(
    blink::WebScreenOrientationLockType lock_orientation) {
  if (!display::Display::HasInternalDisplay())
    return;

  display::Display::Rotation rotation =
      WmShell::Get()
          ->GetDisplayInfo(display::Display::InternalDisplayId())
          .GetActiveRotation();
  if (natural_orientation_ == lock_orientation) {
    if (rotation == display::Display::ROTATE_0 ||
        rotation == display::Display::ROTATE_180) {
      SetRotationLockedInternal(true);
    } else {
      LockRotation(display::Display::ROTATE_0,
                   display::Display::ROTATION_SOURCE_ACTIVE);
    }
  } else {
    if (rotation == display::Display::ROTATE_90 ||
        rotation == display::Display::ROTATE_270) {
      SetRotationLockedInternal(true);
    } else {
      LockRotation(display::Display::ROTATE_90,
                   display::Display::ROTATION_SOURCE_ACTIVE);
    }
  }
}

void ScreenOrientationController::HandleScreenRotation(
    const chromeos::AccelerometerReading& lid) {
  gfx::Vector3dF lid_flattened(lid.x, lid.y, 0.0f);
  float lid_flattened_length = lid_flattened.Length();
  // When the lid is close to being flat, don't change rotation as it is too
  // sensitive to slight movements.
  if (lid_flattened_length < kMinimumAccelerationScreenRotation)
    return;

  // The reference vector is the angle of gravity when the device is rotated
  // clockwise by 45 degrees. Computing the angle between this vector and
  // gravity we can easily determine the expected display rotation.
  static const gfx::Vector3dF rotation_reference(-1.0f, 1.0f, 0.0f);

  // Set the down vector to match the expected direction of gravity given the
  // last configured rotation. This is used to enforce a stickiness that the
  // user must overcome to rotate the display and prevents frequent rotations
  // when holding the device near 45 degrees.
  gfx::Vector3dF down(0.0f, 0.0f, 0.0f);
  if (current_rotation_ == display::Display::ROTATE_0)
    down.set_y(1.0f);
  else if (current_rotation_ == display::Display::ROTATE_90)
    down.set_x(1.0f);
  else if (current_rotation_ == display::Display::ROTATE_180)
    down.set_y(-1.0f);
  else
    down.set_x(-1.0f);

  // Don't rotate if the screen has not passed the threshold.
  if (gfx::AngleBetweenVectorsInDegrees(down, lid_flattened) <
      kDisplayRotationStickyAngleDegrees) {
    return;
  }

  float angle = gfx::ClockwiseAngleBetweenVectorsInDegrees(
      rotation_reference, lid_flattened, gfx::Vector3dF(0.0f, 0.0f, 1.0f));

  display::Display::Rotation new_rotation = display::Display::ROTATE_270;
  if (angle < 90.0f)
    new_rotation = display::Display::ROTATE_0;
  else if (angle < 180.0f)
    new_rotation = display::Display::ROTATE_90;
  else if (angle < 270.0f)
    new_rotation = display::Display::ROTATE_180;

  if (new_rotation != current_rotation_ &&
      IsRotationAllowedInLockedState(new_rotation)) {
    SetDisplayRotation(new_rotation,
                       display::Display::ROTATION_SOURCE_ACCELEROMETER);
  }
}

void ScreenOrientationController::LoadDisplayRotationProperties() {
  display::DisplayManager* display_manager =
      Shell::GetInstance()->display_manager();
  if (!display_manager->registered_internal_display_rotation_lock())
    return;
  user_locked_orientation_ = RotationToOrientation(
      display_manager->registered_internal_display_rotation());
}

void ScreenOrientationController::ApplyLockForActiveWindow() {
  MruWindowTracker::WindowList mru_windows(
      Shell::GetInstance()->mru_window_tracker()->BuildMruWindowList());

  for (WmWindow* window : mru_windows) {
    if (!window->GetTargetVisibility())
      continue;
    for (auto const& pair : locking_windows_) {
      if (pair.first->GetTargetVisibility() && window->Contains(pair.first)) {
        LockRotationToOrientation(
            ResolveOrientationLock(pair.second, user_locked_orientation_));
        return;
      }
    }
    // The default orientation for all chrome browser/apps windows is
    // ANY, so use the user_locked_orientation_;
    if (window->GetTargetVisibility() &&
        static_cast<AppType>(window->GetAppType()) != AppType::OTHERS) {
      LockRotationToOrientation(user_locked_orientation_);
      return;
    }
  }
  LockRotationToOrientation(user_locked_orientation_);
}

bool ScreenOrientationController::IsRotationAllowedInLockedState(
    display::Display::Rotation rotation) {
  if (!rotation_locked_)
    return true;

  if (!CanRotateInLockedState())
    return false;

  if (natural_orientation_ == rotation_locked_orientation_) {
    return rotation == display::Display::ROTATE_0 ||
           rotation == display::Display::ROTATE_180;
  } else {
    return rotation == display::Display::ROTATE_90 ||
           rotation == display::Display::ROTATE_270;
  }
  return false;
}

bool ScreenOrientationController::CanRotateInLockedState() {
  return rotation_locked_orientation_ ==
             blink::WebScreenOrientationLockLandscape ||
         rotation_locked_orientation_ ==
             blink::WebScreenOrientationLockPortrait;
}

}  // namespace ash
