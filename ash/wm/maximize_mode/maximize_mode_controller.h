// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_CONTROLLER_H_
#define ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell_observer.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/vector3d_f.h"

#if defined(OS_CHROMEOS)
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "chromeos/dbus/power_manager_client.h"
#endif  // OS_CHROMEOS

namespace base {
class TickClock;
}

namespace gfx {
class Vector3dF;
}

namespace ui {
class EventHandler;
}

namespace ash {

class MaximizeModeControllerTest;
class ScopedDisableInternalMouseAndKeyboard;
class MaximizeModeWindowManager;
class MaximizeModeWindowManagerTest;
namespace test {
class MultiUserWindowManagerChromeOSTest;
class VirtualKeyboardControllerTest;
}

// MaximizeModeController listens to accelerometer events and automatically
// enters and exits maximize mode when the lid is opened beyond the triggering
// angle and rotates the display to match the device when in maximize mode.
class ASH_EXPORT MaximizeModeController :
#if defined(OS_CHROMEOS)
    public chromeos::AccelerometerReader::Observer,
    public chromeos::PowerManagerClient::Observer,
#endif  // OS_CHROMEOS
    public ShellObserver,
    public WindowTreeHostManager::Observer {
 public:
  MaximizeModeController();
  ~MaximizeModeController() override;

  // True if it is possible to enter maximize mode in the current
  // configuration. If this returns false, it should never be the case that
  // maximize mode becomes enabled.
  bool CanEnterMaximizeMode();

  // TODO(jonross): Merge this with EnterMaximizeMode. Currently these are
  // separate for several reasons: there is no internal display when running
  // unittests; the event blocker prevents keyboard input when running ChromeOS
  // on linux. http://crbug.com/362881
  // Turn the always maximize mode window manager on or off.
  void EnableMaximizeModeWindowManager(bool should_enable);

  // Test if the MaximizeModeWindowManager is enabled or not.
  bool IsMaximizeModeWindowManagerEnabled() const;

  // Add a special window to the MaximizeModeWindowManager for tracking. This is
  // only required for special windows which are handled by other window
  // managers like the |MultiUserWindowManager|.
  // If the maximize mode is not enabled no action will be performed.
  void AddWindow(aura::Window* window);

  // ShellObserver:
  void OnAppTerminating() override;
  void OnMaximizeModeStarted() override;
  void OnMaximizeModeEnded() override;

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

#if defined(OS_CHROMEOS)
  // chromeos::AccelerometerReader::Observer:
  void OnAccelerometerUpdated(
      scoped_refptr<const chromeos::AccelerometerUpdate> update) override;

  // PowerManagerClient::Observer:
  void LidEventReceived(bool open, const base::TimeTicks& time) override;
  void SuspendImminent() override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;
#endif  // OS_CHROMEOS

 private:
  friend class MaximizeModeControllerTest;
  friend class MaximizeModeWindowManagerTest;
  friend class test::MultiUserWindowManagerChromeOSTest;
  friend class test::VirtualKeyboardControllerTest;

  // Used for recording metrics for intervals of time spent in
  // and out of TouchView.
  enum TouchViewIntervalType {
    TOUCH_VIEW_INTERVAL_INACTIVE,
    TOUCH_VIEW_INTERVAL_ACTIVE
  };

  // Set the TickClock. This is only to be used by tests that need to
  // artificially and deterministically control the current time.
  void SetTickClockForTest(std::unique_ptr<base::TickClock> tick_clock);

#if defined(OS_CHROMEOS)
  // Detect hinge rotation from base and lid accelerometers and automatically
  // start / stop maximize mode.
  void HandleHingeRotation(
      scoped_refptr<const chromeos::AccelerometerUpdate> update);
#endif

  // Returns true if the lid was recently opened.
  bool WasLidOpenedRecently() const;

  // Enables MaximizeModeWindowManager, and determines the current state of
  // rotation lock.
  void EnterMaximizeMode();

  // Removes MaximizeModeWindowManager and resets the display rotation if there
  // is no rotation lock.
  void LeaveMaximizeMode();

  // Record UMA stats tracking TouchView usage. If |type| is
  // TOUCH_VIEW_INTERVAL_INACTIVE, then record that TouchView has been
  // inactive from |touchview_usage_interval_start_time_| until now.
  // Similarly, record that TouchView has been active if |type| is
  // TOUCH_VIEW_INTERVAL_ACTIVE.
  void RecordTouchViewUsageInterval(TouchViewIntervalType type);

  // Returns TOUCH_VIEW_INTERVAL_ACTIVE if TouchView is currently active,
  // otherwise returns TOUCH_VIEW_INTERNAL_INACTIVE.
  TouchViewIntervalType CurrentTouchViewIntervalType();

  // The maximized window manager (if enabled).
  std::unique_ptr<MaximizeModeWindowManager> maximize_mode_window_manager_;

  // A helper class which when instantiated will block native events from the
  // internal keyboard and touchpad.
  std::unique_ptr<ScopedDisableInternalMouseAndKeyboard> event_blocker_;

  // Whether we have ever seen accelerometer data.
  bool have_seen_accelerometer_data_;

  // True when the hinge angle has been detected past 180 degrees.
  bool lid_open_past_180_;

  // Tracks time spent in (and out of) touchview mode.
  base::Time touchview_usage_interval_start_time_;
  base::TimeDelta total_touchview_time_;
  base::TimeDelta total_non_touchview_time_;

  // Tracks the last time we received a lid open event. This is used to suppress
  // erroneous accelerometer readings as the lid is opened but the accelerometer
  // reports readings that make the lid to appear near fully open.
  base::TimeTicks last_lid_open_time_;

  // Source for the current time in base::TimeTicks.
  std::unique_ptr<base::TickClock> tick_clock_;

  // Tracks when the lid is closed. Used to prevent entering maximize mode.
  bool lid_is_closed_;

  // Tracks smoothed accelerometer data over time. This is done when the hinge
  // is approaching vertical to remove abrupt acceleration that can lead to
  // incorrect calculations of hinge angles.
  gfx::Vector3dF base_smoothed_;
  gfx::Vector3dF lid_smoothed_;

  DISALLOW_COPY_AND_ASSIGN(MaximizeModeController);
};

}  // namespace ash

#endif  // ASH_WM_MAXIMIZE_MODE_MAXIMIZE_MODE_CONTROLLER_H_
