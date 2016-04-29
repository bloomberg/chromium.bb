// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_USER_METRICS_RECORDER_H_
#define ASH_METRICS_USER_METRICS_RECORDER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/metrics/task_switch_metrics_recorder.h"
#include "base/macros.h"
#include "base/timer/timer.h"

namespace ash {

class DesktopTaskSwitchMetricRecorder;

namespace test {
class UserMetricsRecorderTestAPI;
}

namespace wm {
enum class WmUserMetricsAction;
}

enum UserMetricsAction {
  UMA_ACCEL_EXIT_FIRST_Q,
  UMA_ACCEL_EXIT_SECOND_Q,
  UMA_ACCEL_KEYBOARD_BRIGHTNESS_DOWN_F6,
  UMA_ACCEL_KEYBOARD_BRIGHTNESS_UP_F7,
  UMA_ACCEL_LOCK_SCREEN_LOCK_BUTTON,
  UMA_ACCEL_LOCK_SCREEN_POWER_BUTTON,
  UMA_ACCEL_MAXIMIZE_RESTORE_F4,
  UMA_ACCEL_PREVWINDOW_F5,
  UMA_ACCEL_RESTART_POWER_BUTTON,
  UMA_ACCEL_SHUT_DOWN_POWER_BUTTON,
  UMA_CLOSE_THROUGH_CONTEXT_MENU,
  UMA_DESKTOP_SWITCH_TASK,
  UMA_LAUNCHER_BUTTON_PRESSED_WITH_MOUSE,
  UMA_LAUNCHER_BUTTON_PRESSED_WITH_TOUCH,
  UMA_LAUNCHER_CLICK_ON_APP,
  UMA_LAUNCHER_CLICK_ON_APPLIST_BUTTON,
  UMA_LAUNCHER_LAUNCH_TASK,
  UMA_LAUNCHER_MINIMIZE_TASK,
  UMA_LAUNCHER_SWITCH_TASK,
  UMA_MAXIMIZE_MODE_DISABLED,
  UMA_MAXIMIZE_MODE_ENABLED,
  UMA_MAXIMIZE_MODE_INITIALLY_DISABLED,
  UMA_MOUSE_DOWN,
  UMA_PANEL_MINIMIZE_CAPTION_CLICK,
  UMA_PANEL_MINIMIZE_CAPTION_GESTURE,
  UMA_SHELF_ALIGNMENT_SET_BOTTOM,
  UMA_SHELF_ALIGNMENT_SET_LEFT,
  UMA_SHELF_ALIGNMENT_SET_RIGHT,
  UMA_STATUS_AREA_AUDIO_CURRENT_INPUT_DEVICE,
  UMA_STATUS_AREA_AUDIO_CURRENT_OUTPUT_DEVICE,
  UMA_STATUS_AREA_AUDIO_SWITCH_INPUT_DEVICE,
  UMA_STATUS_AREA_AUDIO_SWITCH_OUTPUT_DEVICE,
  UMA_STATUS_AREA_BRIGHTNESS_CHANGED,
  UMA_STATUS_AREA_BLUETOOTH_CONNECT_KNOWN_DEVICE,
  UMA_STATUS_AREA_BLUETOOTH_CONNECT_UNKNOWN_DEVICE,
  UMA_STATUS_AREA_BLUETOOTH_DISABLED,
  UMA_STATUS_AREA_BLUETOOTH_ENABLED,
  UMA_STATUS_AREA_CAPS_LOCK_DETAILED,
  UMA_STATUS_AREA_CAPS_LOCK_DISABLED_BY_CLICK,
  UMA_STATUS_AREA_CAPS_LOCK_ENABLED_BY_CLICK,
  UMA_STATUS_AREA_CAPS_LOCK_POPUP,
  UMA_STATUS_AREA_CAST_STOP_CAST,
  UMA_STATUS_AREA_CONNECT_TO_CONFIGURED_NETWORK,
  UMA_STATUS_AREA_CONNECT_TO_UNCONFIGURED_NETWORK,
  UMA_STATUS_AREA_CONNECT_TO_VPN,
  UMA_STATUS_AREA_CHANGED_VOLUME_MENU,
  UMA_STATUS_AREA_CHANGED_VOLUME_POPUP,
  UMA_STATUS_AREA_DETAILED_ACCESSABILITY,
  UMA_STATUS_AREA_DETAILED_AUDIO_VIEW,
  UMA_STATUS_AREA_DETAILED_BLUETOOTH_VIEW,
  UMA_STATUS_AREA_DETAILED_BRIGHTNESS_VIEW,
  UMA_STATUS_AREA_DETAILED_CAST_VIEW,
  UMA_STATUS_AREA_DETAILED_CAST_VIEW_LAUNCH_CAST,
  UMA_STATUS_AREA_DETAILED_DRIVE_VIEW,
  UMA_STATUS_AREA_DETAILED_NETWORK_VIEW,
  UMA_STATUS_AREA_DETAILED_VPN_VIEW,
  UMA_STATUS_AREA_DISABLE_AUTO_CLICK,
  UMA_STATUS_AREA_DISABLE_HIGH_CONTRAST,
  UMA_STATUS_AREA_DISABLE_LARGE_CURSOR,
  UMA_STATUS_AREA_DISABLE_MAGNIFIER,
  UMA_STATUS_AREA_DISABLE_SPOKEN_FEEDBACK,
  UMA_STATUS_AREA_DISABLE_WIFI,
  UMA_STATUS_AREA_DISABLE_VIRTUAL_KEYBOARD,
  UMA_STATUS_AREA_DRIVE_CANCEL_OPERATION,
  UMA_STATUS_AREA_DRIVE_SETTINGS,
  UMA_STATUS_AREA_ENABLE_AUTO_CLICK,
  UMA_STATUS_AREA_ENABLE_HIGH_CONTRAST,
  UMA_STATUS_AREA_ENABLE_LARGE_CURSOR,
  UMA_STATUS_AREA_ENABLE_MAGNIFIER,
  UMA_STATUS_AREA_ENABLE_SPOKEN_FEEDBACK,
  UMA_STATUS_AREA_ENABLE_WIFI,
  UMA_STATUS_AREA_ENABLE_VIRTUAL_KEYBOARD,
  UMA_STATUS_AREA_IME_SHOW_DETAILED,
  UMA_STATUS_AREA_IME_SWITCH_MODE,
  UMA_STATUS_AREA_MENU_OPENED,
  UMA_STATUS_AREA_NETWORK_JOIN_OTHER_CLICKED,
  UMA_STATUS_AREA_NETWORK_SETTINGS_CLICKED,
  UMA_STATUS_AREA_SHOW_NETWORK_CONNECTION_DETAILS,
  UMA_STATUS_AREA_SHOW_VPN_CONNECTION_DETAILS,
  UMA_STATUS_AREA_SIGN_OUT,
  UMA_STATUS_AREA_VPN_ADD_BUILT_IN_CLICKED,
  UMA_STATUS_AREA_VPN_ADD_THIRD_PARTY_CLICKED,
  UMA_STATUS_AREA_VPN_DISCONNECT_CLICKED,
  UMA_STATUS_AREA_VPN_SETTINGS_CLICKED,
  UMA_TOGGLE_MAXIMIZE_CAPTION_CLICK,
  UMA_TOGGLE_MAXIMIZE_CAPTION_GESTURE,
  UMA_TOGGLE_SINGLE_AXIS_MAXIMIZE_BORDER_CLICK,
  UMA_TOUCHPAD_GESTURE_OVERVIEW,
  UMA_TOUCHSCREEN_TAP_DOWN,
  UMA_TRAY_HELP,
  UMA_TRAY_LOCK_SCREEN,
  UMA_TRAY_OVERVIEW,
  UMA_TRAY_SHUT_DOWN,
  UMA_WINDOW_APP_CLOSE_BUTTON_CLICK,
  UMA_WINDOW_CLOSE_BUTTON_CLICK,
  UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_EXIT_FULLSCREEN,
  UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_MAXIMIZE,
  UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_MINIMIZE,
  UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_RESTORE,
  UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_LEFT,
  UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_RIGHT,

  // Thumbnail sized overview of windows triggered by pressing the overview key.
  UMA_WINDOW_OVERVIEW,

  // User selected a window in overview mode different from the
  // previously-active window.
  UMA_WINDOW_OVERVIEW_ACTIVE_WINDOW_CHANGED,

  // Selecting a window in overview mode by pressing the enter key.
  UMA_WINDOW_OVERVIEW_ENTER_KEY,

  // Window selection started by beginning an alt+tab cycle. This does not count
  // each step through an alt+tab cycle.
  UMA_WINDOW_CYCLE,
};

// User Metrics Recorder provides a repeating callback (RecordPeriodicMetrics)
// on a timer to allow recording of state data over time to the UMA records.
// Any additional states (in ash) that require monitoring can be added to
// this class. As well calls to record on action metrics
// (RecordUserMetricsAction) are passed through the UserMetricsRecorder.
class ASH_EXPORT UserMetricsRecorder {
 public:
  // Creates a UserMetricsRecorder that records metrics periodically. Equivalent
  // to calling UserMetricsRecorder(true).
  UserMetricsRecorder();

  virtual ~UserMetricsRecorder();

  // Records an Ash owned user action.
  void RecordUserMetricsAction(ash::UserMetricsAction action);
  void RecordUserMetricsAction(wm::WmUserMetricsAction action);

  TaskSwitchMetricsRecorder& task_switch_metrics_recorder() {
    return task_switch_metrics_recorder_;
  }

  // Informs |this| that the Shell has been initialized.
  void OnShellInitialized();

  // Informs |this| that the Shell is going to be shut down.
  void OnShellShuttingDown();

 private:
  friend class test::UserMetricsRecorderTestAPI;

  // Creates a UserMetricsRecorder and will only record periodic metrics if
  // |record_periodic_metrics| is true. This is used by tests that do not want
  // the timer to be started.
  // TODO(bruthig): Add a constructor that accepts a base::RepeatingTimer so
  // that tests can inject a test double that can be controlled by the test. The
  // missing piece is a suitable base::RepeatingTimer test double.
  explicit UserMetricsRecorder(bool record_periodic_metrics);

  // Records UMA metrics. Invoked periodically by the |timer_|.
  void RecordPeriodicMetrics();

  // Returns true if the user's session is active and they are in a desktop
  // environment.
  bool IsUserInActiveDesktopEnvironment() const;

  // Starts the |timer_| and binds it to |RecordPeriodicMetrics|.
  void StartTimer();

  // The periodic timer that triggers metrics to be recorded.
  base::RepeatingTimer timer_;

  TaskSwitchMetricsRecorder task_switch_metrics_recorder_;

  // Metric recorder to track how often task windows are activated by mouse
  // clicks or touchscreen taps.
  std::unique_ptr<DesktopTaskSwitchMetricRecorder>
      desktop_task_switch_metric_recorder_;

  DISALLOW_COPY_AND_ASSIGN(UserMetricsRecorder);
};

}  // namespace ash

#endif  // ASH_METRICS_USER_METRICS_RECORDER_H_
