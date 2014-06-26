// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_CONTROLLER_H_
#define ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_CONTROLLER_H_

#include "ash/accelerometer/accelerometer_observer.h"
#include "ash/ash_export.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/shell_observer.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/power_monitor/power_observer.h"
#include "ui/gfx/display.h"

namespace ui {
class EventHandler;
}

namespace ash {

class MaximizeModeControllerTest;
class ScopedDisableInternalMouseAndKeyboard;
class MaximizeModeWindowManager;
class MaximizeModeWindowManagerTest;

// MaximizeModeController listens to accelerometer events and automatically
// enters and exits maximize mode when the lid is opened beyond the triggering
// angle and rotates the display to match the device when in maximize mode.
class ASH_EXPORT MaximizeModeController : public AccelerometerObserver,
    public base::PowerObserver,
    public ShellObserver,
    public DisplayController::Observer {
 public:
  // Observer that reports changes to the state of MaximizeModeController's
  // rotation lock.
  class Observer {
   public:
    // Invoked whenever |rotation_locked_| is changed.
    virtual void OnRotationLockChanged(bool rotation_locked) {}

   protected:
    virtual ~Observer() {}
  };

  MaximizeModeController();
  virtual ~MaximizeModeController();

  bool in_set_screen_rotation() const {
    return in_set_screen_rotation_;
  }

  // True if |rotation_lock_| has been set, and OnAccelerometerUpdated will not
  // change the display rotation.
  bool rotation_locked() {
    return rotation_locked_;
  }

  // If |rotation_locked| future calls to OnAccelerometerUpdated will not
  // change the display rotation.
  void SetRotationLocked(bool rotation_locked);

  // Add/Remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // True if it is possible to enter maximize mode in the current
  // configuration. If this returns false, it should never be the case that
  // maximize mode becomes enabled.
  bool CanEnterMaximizeMode();

  // TODO(jonross): Merge this with EnterMaximizeMode. Currently these are
  // separate for several reasons: there is no internal display when running
  // unittests; the event blocker prevents keyboard input when running ChromeOS
  // on linux. http://crbug.com/362881
  // Turn the always maximize mode window manager on or off.
  void EnableMaximizeModeWindowManager(bool enable);

  // Test if the MaximizeModeWindowManager is enabled or not.
  bool IsMaximizeModeWindowManagerEnabled() const;

  // TODO(jonross): move this into the destructor. Currently separated as
  // ShellOberver notifies of maximize mode ending, and the observers end up
  // attempting to access MaximizeModeController via the Shell. If done in
  // destructor the controller is null, and the observers segfault.
  // Shuts down down the MaximizeModeWindowManager and notifies all observers.
  void Shutdown();

  // AccelerometerObserver:
  virtual void OnAccelerometerUpdated(const gfx::Vector3dF& base,
                                      const gfx::Vector3dF& lid) OVERRIDE;

  // ShellObserver:
  virtual void OnAppTerminating() OVERRIDE;
  virtual void OnMaximizeModeStarted() OVERRIDE;
  virtual void OnMaximizeModeEnded() OVERRIDE;

  // base::PowerObserver:
  virtual void OnSuspend() OVERRIDE;
  virtual void OnResume() OVERRIDE;

  // DisplayController::Observer:
  virtual void OnDisplayConfigurationChanged() OVERRIDE;

 private:
  friend class MaximizeModeControllerTest;
  friend class MaximizeModeWindowManagerTest;

  // Detect hinge rotation from |base| and |lid| accelerometers and
  // automatically start / stop maximize mode.
  void HandleHingeRotation(const gfx::Vector3dF& base,
                           const gfx::Vector3dF& lid);

  // Detect screen rotation from |lid| accelerometer and automatically rotate
  // screen.
  void HandleScreenRotation(const gfx::Vector3dF& lid);

  // Sets the display rotation and suppresses display notifications.
  void SetDisplayRotation(DisplayManager* display_manager,
                          gfx::Display::Rotation rotation);

  // Enables MaximizeModeWindowManager, and determines the current state of
  // rotation lock.
  void EnterMaximizeMode();

  // Removes MaximizeModeWindowManager and resets the display rotation if there
  // is no rotation lock.
  void LeaveMaximizeMode();

  // Record UMA stats tracking touchview usage.
  void RecordTouchViewStateTransition();

  // The maximized window manager (if enabled).
  scoped_ptr<MaximizeModeWindowManager> maximize_mode_window_manager_;

  // A helper class which when instantiated will block native events from the
  // internal keyboard and touchpad.
  scoped_ptr<ScopedDisableInternalMouseAndKeyboard> event_blocker_;

  // An event handler used to detect screenshot actions while in maximize mode.
  scoped_ptr<ui::EventHandler> event_handler_;

  // When true calls to OnAccelerometerUpdated will not rotate the display.
  bool rotation_locked_;

  // Whether we have ever seen accelerometer data.
  bool have_seen_accelerometer_data_;

  // True when the screen's orientation is being changed.
  bool in_set_screen_rotation_;

  // The rotation of the display set by the user. This rotation will be
  // restored upon exiting maximize mode.
  gfx::Display::Rotation user_rotation_;

  // The current rotation set by MaximizeModeController for the internal
  // display. Compared in OnDisplayConfigurationChanged to determine user
  // display setting changes.
  gfx::Display::Rotation current_rotation_;

  // Rotation Lock observers.
  ObserverList<Observer> observers_;

  // Tracks time spent in (and out of) touchview mode.
  base::Time last_touchview_transition_time_;
  base::TimeDelta total_touchview_time_;
  base::TimeDelta total_non_touchview_time_;

  DISALLOW_COPY_AND_ASSIGN(MaximizeModeController);
};

}  // namespace ash

#endif  // ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_CONTROLLER_H_
