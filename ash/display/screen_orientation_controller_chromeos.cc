// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/screen_orientation_controller_chromeos.h"

#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "ui/aura/client/aura_constants.h"
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
    return blink::kWebScreenOrientationLockLandscape;

  display::ManagedDisplayInfo info =
      Shell::Get()->display_manager()->GetDisplayInfo(
          display::Display::InternalDisplayId());
  gfx::Size size = info.size_in_pixel();
  switch (info.GetActiveRotation()) {
    case display::Display::ROTATE_0:
    case display::Display::ROTATE_180:
      return size.height() >= size.width()
                 ? blink::kWebScreenOrientationLockPortrait
                 : blink::kWebScreenOrientationLockLandscape;
    case display::Display::ROTATE_90:
    case display::Display::ROTATE_270:
      return size.height() < size.width()
                 ? blink::kWebScreenOrientationLockPortrait
                 : blink::kWebScreenOrientationLockLandscape;
  }
  NOTREACHED();
  return blink::kWebScreenOrientationLockLandscape;
}

blink::WebScreenOrientationLockType RotationToOrientation(
    display::Display::Rotation rotation) {
  blink::WebScreenOrientationLockType natural = GetDisplayNaturalOrientation();
  if (natural == blink::kWebScreenOrientationLockLandscape) {
    switch (rotation) {
      case display::Display::ROTATE_0:
        return blink::kWebScreenOrientationLockLandscapePrimary;
      case display::Display::ROTATE_90:
        return blink::kWebScreenOrientationLockPortraitPrimary;
      case display::Display::ROTATE_180:
        return blink::kWebScreenOrientationLockLandscapeSecondary;
      case display::Display::ROTATE_270:
        return blink::kWebScreenOrientationLockPortraitSecondary;
    }
  } else {  // Natural portrait
    switch (rotation) {
      case display::Display::ROTATE_0:
        return blink::kWebScreenOrientationLockPortraitPrimary;
      case display::Display::ROTATE_90:
        return blink::kWebScreenOrientationLockLandscapePrimary;
      case display::Display::ROTATE_180:
        return blink::kWebScreenOrientationLockPortraitSecondary;
      case display::Display::ROTATE_270:
        return blink::kWebScreenOrientationLockLandscapeSecondary;
    }
  }
  NOTREACHED();
  return blink::kWebScreenOrientationLockAny;
}

// Returns the rotation that matches the orientation type.
// Returns ROTATE_0 if the given orientation is ANY, which is used
// to indicate that user didn't lock orientation.
display::Display::Rotation OrientationToRotation(
    blink::WebScreenOrientationLockType orientation) {
  blink::WebScreenOrientationLockType natural = GetDisplayNaturalOrientation();
  if (orientation == blink::kWebScreenOrientationLockAny) {
    return display::Display::ROTATE_0;
  }

  if (natural == blink::kWebScreenOrientationLockLandscape) {
    switch (orientation) {
      case blink::kWebScreenOrientationLockLandscapePrimary:
        return display::Display::ROTATE_0;
      case blink::kWebScreenOrientationLockPortraitPrimary:
        return display::Display::ROTATE_90;
      case blink::kWebScreenOrientationLockLandscapeSecondary:
        return display::Display::ROTATE_180;
      case blink::kWebScreenOrientationLockPortraitSecondary:
        return display::Display::ROTATE_270;
      default:
        break;
    }
  } else {  // Natural portrait
    switch (orientation) {
      case blink::kWebScreenOrientationLockPortraitPrimary:
        return display::Display::ROTATE_0;
      case blink::kWebScreenOrientationLockLandscapePrimary:
        return display::Display::ROTATE_90;
      case blink::kWebScreenOrientationLockPortraitSecondary:
        return display::Display::ROTATE_180;
      case blink::kWebScreenOrientationLockLandscapeSecondary:
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
  if (app_requested == blink::kWebScreenOrientationLockAny ||
      (app_requested == blink::kWebScreenOrientationLockLandscape &&
       (lock == blink::kWebScreenOrientationLockLandscapePrimary ||
        lock == blink::kWebScreenOrientationLockLandscapeSecondary)) ||
      (app_requested == blink::kWebScreenOrientationLockPortrait &&
       (lock == blink::kWebScreenOrientationLockPortraitPrimary ||
        lock == blink::kWebScreenOrientationLockPortraitSecondary))) {
    return lock;
  }
  return app_requested;
}

}  // namespace

ScreenOrientationController::ScreenOrientationController()
    : natural_orientation_(GetDisplayNaturalOrientation()),
      ignore_display_configuration_updates_(false),
      rotation_locked_(false),
      rotation_locked_orientation_(blink::kWebScreenOrientationLockAny),
      user_rotation_(display::Display::ROTATE_0),
      current_rotation_(display::Display::ROTATE_0) {
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
}

ScreenOrientationController::~ScreenOrientationController() {
  Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
  chromeos::AccelerometerReader::GetInstance()->RemoveObserver(this);
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
  Shell::Get()->activation_client()->RemoveObserver(this);
  for (auto& windows : lock_info_map_)
    windows.first->RemoveObserver(this);
}

void ScreenOrientationController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ScreenOrientationController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ScreenOrientationController::LockOrientationForWindow(
    aura::Window* requesting_window,
    blink::WebScreenOrientationLockType lock_orientation,
    LockCompletionBehavior lock_completion_behavior) {
  if (lock_info_map_.empty())
    Shell::Get()->activation_client()->AddObserver(this);

  if (!requesting_window->HasObserver(this))
    requesting_window->AddObserver(this);
  lock_info_map_[requesting_window] =
      LockInfo(lock_orientation, lock_completion_behavior);

  ApplyLockForActiveWindow();
}

void ScreenOrientationController::UnlockOrientationForWindow(
    aura::Window* window) {
  lock_info_map_.erase(window);
  if (lock_info_map_.empty())
    Shell::Get()->activation_client()->RemoveObserver(this);
  window->RemoveObserver(this);
  ApplyLockForActiveWindow();
}

void ScreenOrientationController::UnlockAll() {
  for (auto pair : lock_info_map_)
    pair.first->RemoveObserver(this);
  lock_info_map_.clear();
  Shell::Get()->activation_client()->RemoveObserver(this);
  SetRotationLockedInternal(false);
  if (user_rotation_ != current_rotation_) {
    SetDisplayRotation(user_rotation_,
                       display::Display::ROTATION_SOURCE_ACCELEROMETER,
                       DisplayConfigurationController::ANIMATION_SYNC);
  }
}

bool ScreenOrientationController::ScreenOrientationProviderSupported() const {
  return Shell::Get()
      ->tablet_mode_controller()
      ->IsTabletModeWindowManagerEnabled();
}

bool ScreenOrientationController::IsUserLockedOrientationPortrait() {
  switch (user_locked_orientation_) {
    case blink::kWebScreenOrientationLockPortraitPrimary:
    case blink::kWebScreenOrientationLockPortraitSecondary:
    case blink::kWebScreenOrientationLockPortrait:
      return true;
    default:
      return false;
  }
}

void ScreenOrientationController::ToggleUserRotationLock() {
  if (!display::Display::HasInternalDisplay())
    return;

  if (user_rotation_locked()) {
    SetLockToOrientation(blink::kWebScreenOrientationLockAny);
  } else {
    display::Display::Rotation current_rotation =
        Shell::Get()
            ->display_manager()
            ->GetDisplayInfo(display::Display::InternalDisplayId())
            .GetActiveRotation();
    SetLockToRotation(current_rotation);
  }
}

void ScreenOrientationController::SetLockToRotation(
    display::Display::Rotation rotation) {
  if (!display::Display::HasInternalDisplay())
    return;

  SetLockToOrientation(RotationToOrientation(rotation));
}

blink::WebScreenOrientationLockType
ScreenOrientationController::GetCurrentOrientation() const {
  return RotationToOrientation(current_rotation_);
}

void ScreenOrientationController::OnWindowActivated(
    ::wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  ApplyLockForActiveWindow();
}

void ScreenOrientationController::OnWindowDestroying(aura::Window* window) {
  UnlockOrientationForWindow(window);
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
  if (lock_info_map_.find(window) == lock_info_map_.end())
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
  if (!Shell::Get()->display_manager()->IsActiveDisplayId(
          display::Display::InternalDisplayId())) {
    return;
  }
  // TODO(oshima): remove current_rotation_ and always use the target rotation.
  current_rotation_ =
      Shell::Get()->display_configuration_controller()->GetTargetRotation(
          display::Display::InternalDisplayId());
}

void ScreenOrientationController::OnTabletModeStarted() {
  Shell* shell = Shell::Get();
  // Do not exit early, as the internal display can be determined after Maximize
  // Mode has started. (chrome-os-partner:38796)
  // Always start observing.
  if (display::Display::HasInternalDisplay()) {
    current_rotation_ = user_rotation_ =
        shell->display_configuration_controller()->GetTargetRotation(
            display::Display::InternalDisplayId());
  }
  if (!rotation_locked_)
    LoadDisplayRotationProperties();
  chromeos::AccelerometerReader::GetInstance()->AddObserver(this);
  shell->window_tree_host_manager()->AddObserver(this);

  if (!display::Display::HasInternalDisplay())
    return;
  ApplyLockForActiveWindow();
  for (auto& observer : observers_)
    observer.OnUserRotationLockChanged();
}

void ScreenOrientationController::OnTabletModeEnding() {
  chromeos::AccelerometerReader::GetInstance()->RemoveObserver(this);
  Shell::Get()->window_tree_host_manager()->RemoveObserver(this);
  if (!display::Display::HasInternalDisplay())
    return;

  // TODO(oshima): Remove if when current_rotation_ is removed.
  if (current_rotation_ != user_rotation_) {
    SetDisplayRotation(user_rotation_,
                       display::Display::ROTATION_SOURCE_ACCELEROMETER,
                       DisplayConfigurationController::ANIMATION_SYNC);
  }
  for (auto& observer : observers_)
    observer.OnUserRotationLockChanged();
}

void ScreenOrientationController::SetDisplayRotation(
    display::Display::Rotation rotation,
    display::Display::RotationSource source,
    DisplayConfigurationController::RotationAnimation mode) {
  if (!display::Display::HasInternalDisplay())
    return;
  current_rotation_ = rotation;
  base::AutoReset<bool> auto_ignore_display_configuration_updates(
      &ignore_display_configuration_updates_, true);

  Shell::Get()->display_configuration_controller()->SetDisplayRotation(
      display::Display::InternalDisplayId(), rotation, source, mode);
}

void ScreenOrientationController::SetRotationLockedInternal(
    bool rotation_locked) {
  if (rotation_locked_ == rotation_locked)
    return;
  rotation_locked_ = rotation_locked;
  if (!rotation_locked_)
    rotation_locked_orientation_ = blink::kWebScreenOrientationLockAny;
}

void ScreenOrientationController::SetLockToOrientation(
    blink::WebScreenOrientationLockType orientation) {
  user_locked_orientation_ = orientation;
  base::AutoReset<bool> auto_ignore_display_configuration_updates(
      &ignore_display_configuration_updates_, true);
  Shell::Get()->display_manager()->RegisterDisplayRotationProperties(
      user_rotation_locked(), OrientationToRotation(user_locked_orientation_));

  ApplyLockForActiveWindow();
  for (auto& observer : observers_)
    observer.OnUserRotationLockChanged();
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
    case blink::kWebScreenOrientationLockAny:
      SetRotationLockedInternal(false);
      break;
    case blink::kWebScreenOrientationLockDefault:
      NOTREACHED();
      break;
    case blink::kWebScreenOrientationLockPortraitPrimary:
      LockRotationToPrimaryOrientation(
          blink::kWebScreenOrientationLockPortrait);
      break;
    case blink::kWebScreenOrientationLockLandscape:
    case blink::kWebScreenOrientationLockPortrait:
      LockToRotationMatchingOrientation(lock_orientation);
      break;
    case blink::kWebScreenOrientationLockPortraitSecondary:
      LockRotationToSecondaryOrientation(
          blink::kWebScreenOrientationLockPortrait);
      break;
    case blink::kWebScreenOrientationLockLandscapeSecondary:
      LockRotationToSecondaryOrientation(
          blink::kWebScreenOrientationLockLandscape);
      break;
    case blink::kWebScreenOrientationLockLandscapePrimary:
      LockRotationToPrimaryOrientation(
          blink::kWebScreenOrientationLockLandscape);
      break;
    case blink::kWebScreenOrientationLockNatural:
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
      Shell::Get()
          ->display_manager()
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
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  if (!display_manager->registered_internal_display_rotation_lock())
    return;
  user_locked_orientation_ = RotationToOrientation(
      display_manager->registered_internal_display_rotation());
}

void ScreenOrientationController::ApplyLockForActiveWindow() {
  MruWindowTracker::WindowList mru_windows(
      Shell::Get()->mru_window_tracker()->BuildMruWindowList());

  for (auto* window : mru_windows) {
    if (!window->TargetVisibility())
      continue;
    for (auto& pair : lock_info_map_) {
      if (pair.first->TargetVisibility() && window->Contains(pair.first)) {
        LockRotationToOrientation(ResolveOrientationLock(
            pair.second.orientation, user_locked_orientation_));
        if (pair.second.lock_completion_behavior ==
            LockCompletionBehavior::DisableSensor) {
          pair.second.orientation = RotationToOrientation(current_rotation_);
          pair.second.lock_completion_behavior = LockCompletionBehavior::None;
          LockRotationToOrientation(pair.second.orientation);
        }
        return;
      }
    }
    // The default orientation for all chrome browser/apps windows is
    // ANY, so use the user_locked_orientation_;
    if (window->TargetVisibility() &&
        static_cast<AppType>(window->GetProperty(aura::client::kAppType)) !=
            AppType::OTHERS) {
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
             blink::kWebScreenOrientationLockLandscape ||
         rotation_locked_orientation_ ==
             blink::kWebScreenOrientationLockPortrait;
}

}  // namespace ash
