// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_CONTROLLER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/interfaces/tablet_mode.mojom.h"
#include "ash/session/session_observer.h"
#include "ash/shell_observer.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
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
class TabletModeObserver;
class TabletModeWindowManager;
class TabletModeWindowManagerTest;

// TabletModeController listens to accelerometer events and automatically
// enters and exits tablet mode when the lid is opened beyond the triggering
// angle and rotates the display to match the device when in tablet mode.
class ASH_EXPORT TabletModeController
    : public chromeos::AccelerometerReader::Observer,
      public chromeos::PowerManagerClient::Observer,
      public mojom::TabletModeController,
      public ShellObserver,
      public WindowTreeHostManager::Observer,
      public SessionObserver {
 public:
  // Used for keeping track if the user wants the machine to behave as a
  // clamshell/tabletmode regardless of hardware orientation.
  // TODO(oshima): Move this to common place.
  enum class UiMode {
    NONE = 0,
    CLAMSHELL,
    TABLETMODE,
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

  // Binds the mojom::TabletModeController interface request to this object.
  void BindRequest(mojom::TabletModeControllerRequest request);

  void AddObserver(TabletModeObserver* observer);
  void RemoveObserver(TabletModeObserver* observer);

  // Checks if we should auto hide title bars in tablet mode. Returns true if
  // the feature is enabled and we are in tablet mode.
  bool ShouldAutoHideTitlebars() const;

  bool auto_hide_title_bars() const { return auto_hide_title_bars_; }

  // Flushes the mojo message pipe to chrome.
  void FlushForTesting();

  // ShellObserver:
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
  // and out of TabletMode.
  enum TabletModeIntervalType {
    TABLET_MODE_INTERVAL_INACTIVE,
    TABLET_MODE_INTERVAL_ACTIVE
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

  // Record UMA stats tracking TabletMode usage. If |type| is
  // TABLET_MODE_INTERVAL_INACTIVE, then record that TabletMode has been
  // inactive from |tabletmode_usage_interval_start_time_| until now.
  // Similarly, record that TabletMode has been active if |type| is
  // TABLET_MODE_INTERVAL_ACTIVE.
  void RecordTabletModeUsageInterval(TabletModeIntervalType type);

  // Returns TABLET_MODE_INTERVAL_ACTIVE if TabletMode is currently active,
  // otherwise returns TABLET_MODE_INTERNAL_INACTIVE.
  TabletModeIntervalType CurrentTabletModeIntervalType();

  // mojom::TabletModeController:
  void SetClient(mojom::TabletModeClientPtr client) override;

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

  // Tracks time spent in (and out of) tabletmode mode.
  base::Time tabletmode_usage_interval_start_time_;
  base::TimeDelta total_tabletmode_time_;
  base::TimeDelta total_non_tabletmode_time_;

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

  // Whether title bars should be shown be auto hidden in tablet mode.
  const bool auto_hide_title_bars_ = false;

  // Tracks smoothed accelerometer data over time. This is done when the hinge
  // is approaching vertical to remove abrupt acceleration that can lead to
  // incorrect calculations of hinge angles.
  gfx::Vector3dF base_smoothed_;
  gfx::Vector3dF lid_smoothed_;

  // Bindings for the TabletModeController interface.
  mojo::BindingSet<mojom::TabletModeController> bindings_;

  // Client interface (e.g. in chrome).
  mojom::TabletModeClientPtr client_;

  // Tracks whether a flag is used to force ui mode.
  UiMode force_ui_mode_ = UiMode::NONE;

  ScopedSessionObserver scoped_session_observer_;

  base::ObserverList<TabletModeObserver> tablet_mode_observers_;

  base::WeakPtrFactory<TabletModeController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabletModeController);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_CONTROLLER_H_
