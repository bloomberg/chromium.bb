// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_CHROMEOS_H_
#define ASH_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_CHROMEOS_H_

#include <unordered_map>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationLockType.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}

namespace ash {

// Implements ChromeOS specific functionality for ScreenOrientationProvider.
class ASH_EXPORT ScreenOrientationController
    : public ::wm::ActivationChangeObserver,
      public aura::WindowObserver,
      public chromeos::AccelerometerReader::Observer,
      public WindowTreeHostManager::Observer,
      public ShellObserver {
 public:
  // Observer that reports changes to the state of ScreenOrientationProvider's
  // rotation lock.
  class Observer {
   public:
    // Invoked when rotation is locked or unlocked by a user.
    virtual void OnUserRotationLockChanged() {}

   protected:
    virtual ~Observer() {}
  };

  // Controls the behavior after lock is applied to the window (when
  // the window becomes active window). |DisableSensor| disables
  // the sensor based rotation and locks to the specific orientation.
  // For example, PORTRAIT may rotate to PORTRAIT_PRIMARY or
  // PORTRAIT_SECONDARY, and will allow rotate between these two.
  // |DisableSensor| will lock the orientation to the one of them
  // after locked to disalow the sensor basd rotation.
  enum class LockCompletionBehavior {
    None,
    DisableSensor,
  };

  ScreenOrientationController();
  ~ScreenOrientationController() override;

  // Add/Remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Allows/unallows a window to lock the screen orientation.
  void LockOrientationForWindow(
      aura::Window* requesting_window,
      blink::WebScreenOrientationLockType lock_orientation,
      LockCompletionBehavior lock_completion_behavior);
  void UnlockOrientationForWindow(aura::Window* window);

  // Unlock all and set the rotation back to the user specified rotation.
  void UnlockAll();

  bool ScreenOrientationProviderSupported() const;

  // Returns true if the user has locked the orientation to portrait, false if
  // the user has locked the orientation to landscape or not locked the
  // orientation.
  bool IsUserLockedOrientationPortrait();

  bool ignore_display_configuration_updates() const {
    return ignore_display_configuration_updates_;
  }

  // True if |rotation_lock_| has been set and accelerometer updates should not
  // rotate the display.
  bool rotation_locked() const { return rotation_locked_; }

  bool user_rotation_locked() const {
    return user_locked_orientation_ != blink::kWebScreenOrientationLockAny;
  }

  // Trun on/off the user rotation lock. When turned on, it will lock
  // the orientation to the current orientation.
  // |user_rotation_locked()| method returns the current state of the
  // user rotation lock.
  void ToggleUserRotationLock();

  // Set locked to the given |rotation| and save it.
  void SetLockToRotation(display::Display::Rotation rotation);

  // wm::ActivationChangeObserver:
  void OnWindowActivated(
      ::wm::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  // chromeos::AccelerometerReader::Observer:
  void OnAccelerometerUpdated(
      scoped_refptr<const chromeos::AccelerometerUpdate> update) override;

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // ShellObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnding() override;

 private:
  friend class ScreenOrientationControllerTestApi;

  struct LockInfo {
    LockInfo() {}
    LockInfo(blink::WebScreenOrientationLockType orientation,
             LockCompletionBehavior lock_completion_behavior)
        : orientation(orientation),
          lock_completion_behavior(lock_completion_behavior) {}

    blink::WebScreenOrientationLockType orientation =
        blink::kWebScreenOrientationLockAny;
    LockCompletionBehavior lock_completion_behavior =
        LockCompletionBehavior::None;
  };

  // Sets the display rotation for the given |source|. The new |rotation| will
  // also become active. Display changed notifications are surpressed for this
  // change.
  void SetDisplayRotation(display::Display::Rotation rotation,
                          display::Display::RotationSource source);

  void SetRotationLockedInternal(bool rotation_locked);

  // A helper method that set locked to the given |orientation| and save it.
  void SetLockToOrientation(blink::WebScreenOrientationLockType orientation);

  // Sets the display rotation to |rotation|. Future accelerometer updates
  // should not be used to change the rotation. SetRotationLocked(false) removes
  // the rotation lock.
  void LockRotation(display::Display::Rotation rotation,
                    display::Display::RotationSource source);

  // Sets the display rotation based on |lock_orientation|. Future accelerometer
  // updates should not be used to change the rotation. SetRotationLocked(false)
  // removes the rotation lock.
  void LockRotationToOrientation(
      blink::WebScreenOrientationLockType lock_orientation);

  // Locks rotation to the angle matching the primary orientation for
  // |lock_orientation|.
  void LockRotationToPrimaryOrientation(
      blink::WebScreenOrientationLockType lock_orientation);

  // Locks rotation to the angle matching the secondary orientation for
  // |lock_orientation|.
  void LockRotationToSecondaryOrientation(
      blink::WebScreenOrientationLockType lock_orientation);

  // For orientations that do not specify primary or secondary, locks to the
  // current rotation if it matches |lock_orientation|. Otherwise locks to a
  // matching rotation.
  void LockToRotationMatchingOrientation(
      blink::WebScreenOrientationLockType lock_orientation);

  // Detect screen rotation from |lid| accelerometer and automatically rotate
  // screen.
  void HandleScreenRotation(const chromeos::AccelerometerReading& lid);

  // Checks DisplayManager for registered rotation lock, and rotation,
  // preferences. These are then applied.
  void LoadDisplayRotationProperties();

  // Determines the rotation lock, and orientation, for the currently active
  // window, and applies it. If there is none, rotation lock will be removed.
  void ApplyLockForActiveWindow();

  // Both |blink::WebScreenOrientationLockLandscape| and
  // |blink::WebScreenOrientationLockPortrait| allow for rotation between the
  // two angles of the same screen orientation
  // (http://www.w3.org/TR/screen-orientation/). Returns true if |rotation| is
  // supported for the current |rotation_locked_orientation_|.
  bool IsRotationAllowedInLockedState(display::Display::Rotation rotation);

  blink::WebScreenOrientationLockType GetCurrentOrientationForTest() const;

  // Certain orientation locks allow for rotation between the two angles of the
  // same screen orientation. Returns true if |rotation_locked_orientation_|
  // allows rotation.
  bool CanRotateInLockedState();

  // The orientation of the display when at a rotation of 0.
  blink::WebScreenOrientationLockType natural_orientation_;

  // True when changes being applied cause OnDisplayConfigurationChanged() to be
  // called, and for which these changes should be ignored.
  bool ignore_display_configuration_updates_;

  // When true then accelerometer updates should not rotate the display.
  bool rotation_locked_;

  // The orientation to which the current |rotation_locked_| was applied.
  blink::WebScreenOrientationLockType rotation_locked_orientation_;

  // The rotation of the display set by the user. This rotation will be
  // restored upon exiting tablet mode.
  display::Display::Rotation user_rotation_;

  // The orientation of the device locked by the user.
  blink::WebScreenOrientationLockType user_locked_orientation_ =
      blink::kWebScreenOrientationLockAny;

  // The current rotation set by ScreenOrientationController for the internal
  // display.
  display::Display::Rotation current_rotation_;

  // Rotation Lock observers.
  base::ObserverList<Observer> observers_;

  // Tracks all windows that have requested a lock, as well as the requested
  // orientation.
  std::unordered_map<aura::Window*, LockInfo> lock_info_map_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationController);
};

}  // namespace ash

#endif  // ASH_DISPLAY_SCREEN_ORIENTATION_CONTROLLER_CHROMEOS_H_
