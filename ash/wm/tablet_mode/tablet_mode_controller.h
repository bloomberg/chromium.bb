// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_CONTROLLER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/interfaces/touch_view.mojom.h"
#include "ash/session/session_observer.h"
#include "ash/shell_observer.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/accelerometer/accelerometer_reader.h"
#include "chromeos/accelerometer/accelerometer_types.h"
#include "chromeos/dbus/power_manager_client.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace aura {
class Window;
}

namespace base {
class TickClock;
}

namespace gfx {
class Vector3dF;
}

namespace ash {

class ScopedDisableInternalMouseAndKeyboard;
class TabletModeControllerTest;
class TabletModeWindowManager;
class TabletModeWindowManagerTest;

// TabletModeController listens to accelerometer events and automatically
// enters and exits tablet mode when the lid is opened beyond the triggering
// angle and rotates the display to match the device when in tablet mode.
class ASH_EXPORT TabletModeController
    : public chromeos::AccelerometerReader::Observer,
      public chromeos::PowerManagerClient::Observer,
      NON_EXPORTED_BASE(public mojom::TouchViewManager),
      public ShellObserver,
      public WindowTreeHostManager::Observer,
      public SessionObserver {
 public:
  // Used for keeping track if the user wants the machine to behave as a
  // clamshell/touchview regardless of hardware orientation.
  enum class ForceTabletMode {
    NONE = 0,
    CLAMSHELL,
    TOUCHVIEW,
  };

  TabletModeController();
  ~TabletModeController() override;

  // True if it is possible to enter tablet mode in the current
  // configuration. If this returns false, it should never be the case that
  // tablet mode becomes enabled.
  bool CanEnterTabletMode();

  // TODO(jonross): Merge this with EnterTabletMode. Currently these are
  // separate for several reasons: there is no internal display when running
  // unittests; the event blocker prevents keyboard input when running ChromeOS
  // on linux. http://crbug.com/362881
  // Turn the always tablet mode window manager on or off.
  void EnableTabletModeWindowManager(bool should_enable);

  // Test if the TabletModeWindowManager is enabled or not.
  bool IsTabletModeWindowManagerEnabled() const;

  // Add a special window to the TabletModeWindowManager for tracking. This is
  // only required for special windows which are handled by other window
  // managers like the |MultiUserWindowManager|.
  // If the tablet mode is not enabled no action will be performed.
  void AddWindow(aura::Window* window);

  // Binds the mojom::TouchViewManager interface request to this object.
  void BindRequest(mojom::TouchViewManagerRequest request);

  // ShellObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;
  void OnShellInitialized() override;

  // WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // SessionObserver:
  void OnChromeTerminating() override;

  // chromeos::AccelerometerReader::Observer:
  void OnAccelerometerUpdated(
      scoped_refptr<const chromeos::AccelerometerUpdate> update) override;

  // chromeos::PowerManagerClient::Observer:
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        const base::TimeTicks& time) override;
  void TabletModeEventReceived(chromeos::PowerManagerClient::TabletMode mode,
                               const base::TimeTicks& time) override;
  void SuspendImminent() override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

 private:
  friend class TabletModeControllerTest;
  friend class TabletModeWindowManagerTest;
  friend class MultiUserWindowManagerChromeOSTest;
  friend class VirtualKeyboardControllerTest;

  // Used for recording metrics for intervals of time spent in
  // and out of TouchView.
  enum TouchViewIntervalType {
    TOUCH_VIEW_INTERVAL_INACTIVE,
    TOUCH_VIEW_INTERVAL_ACTIVE
  };

  // Set the TickClock. This is only to be used by tests that need to
  // artificially and deterministically control the current time.
  void SetTickClockForTest(std::unique_ptr<base::TickClock> tick_clock);

  // Detect hinge rotation from base and lid accelerometers and automatically
  // start / stop tablet mode.
  void HandleHingeRotation(
      scoped_refptr<const chromeos::AccelerometerUpdate> update);

  void OnGetSwitchStates(chromeos::PowerManagerClient::LidState lid_state,
                         chromeos::PowerManagerClient::TabletMode tablet_mode);

  // Returns true if the lid was recently opened.
  bool WasLidOpenedRecently() const;

  // Enables TabletModeWindowManager, and determines the current state of
  // rotation lock.
  void EnterTabletMode();

  // Removes TabletModeWindowManager and resets the display rotation if there
  // is no rotation lock.
  void LeaveTabletMode();

  // Record UMA stats tracking TouchView usage. If |type| is
  // TOUCH_VIEW_INTERVAL_INACTIVE, then record that TouchView has been
  // inactive from |touchview_usage_interval_start_time_| until now.
  // Similarly, record that TouchView has been active if |type| is
  // TOUCH_VIEW_INTERVAL_ACTIVE.
  void RecordTouchViewUsageInterval(TouchViewIntervalType type);

  // Returns TOUCH_VIEW_INTERVAL_ACTIVE if TouchView is currently active,
  // otherwise returns TOUCH_VIEW_INTERNAL_INACTIVE.
  TouchViewIntervalType CurrentTouchViewIntervalType();

  // mojom::TouchViewManager:
  void AddObserver(mojom::TouchViewObserverPtr observer) override;

  // Checks whether we want to allow entering and exiting tablet mode. This
  // returns false if the user set a flag for the software to behave in a
  // certain way regardless of configuration.
  bool AllowEnterExitTabletMode() const;

  // The maximized window manager (if enabled).
  std::unique_ptr<TabletModeWindowManager> tablet_mode_window_manager_;

  // A helper class which when instantiated will block native events from the
  // internal keyboard and touchpad.
  std::unique_ptr<ScopedDisableInternalMouseAndKeyboard> event_blocker_;

  // Whether we have ever seen accelerometer data.
  bool have_seen_accelerometer_data_;

  // Whether both accelerometers are available.
  bool can_detect_lid_angle_;

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

  // Set when tablet mode switch is on. This is used to force tablet mode.
  bool tablet_mode_switch_is_on_;

  // Tracks when the lid is closed. Used to prevent entering tablet mode.
  bool lid_is_closed_;

  // Tracks smoothed accelerometer data over time. This is done when the hinge
  // is approaching vertical to remove abrupt acceleration that can lead to
  // incorrect calculations of hinge angles.
  gfx::Vector3dF base_smoothed_;
  gfx::Vector3dF lid_smoothed_;

  // Bindings for the TouchViewManager interface.
  mojo::BindingSet<mojom::TouchViewManager> bindings_;

  // The set of touchview observers to be notified about mode changes.
  mojo::InterfacePtrSet<mojom::TouchViewObserver> observers_;

  // Tracks whether a flag is used to force tablet mode.
  ForceTabletMode force_tablet_mode_ = ForceTabletMode::NONE;

  ScopedSessionObserver scoped_session_observer_;

  base::WeakPtrFactory<TabletModeController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabletModeController);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_CONTROLLER_H_
