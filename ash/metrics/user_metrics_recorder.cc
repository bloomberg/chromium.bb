// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/user_metrics_recorder.h"

#include "ash/metrics/desktop_task_switch_metric_recorder.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_delegate.h"
#include "ash/shelf/shelf_item_types.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

// Time in seconds between calls to "RecordPeriodicMetrics".
const int kAshPeriodicMetricsTimeInSeconds = 30 * 60;

enum ActiveWindowStateType {
  ACTIVE_WINDOW_STATE_TYPE_NO_ACTIVE_WINDOW,
  ACTIVE_WINDOW_STATE_TYPE_OTHER,
  ACTIVE_WINDOW_STATE_TYPE_MAXIMIZED,
  ACTIVE_WINDOW_STATE_TYPE_FULLSCREEN,
  ACTIVE_WINDOW_STATE_TYPE_SNAPPED,
  ACTIVE_WINDOW_STATE_TYPE_DOCKED,
  ACTIVE_WINDOW_STATE_TYPE_COUNT
};

ActiveWindowStateType GetActiveWindowState() {
  ActiveWindowStateType active_window_state_type =
      ACTIVE_WINDOW_STATE_TYPE_NO_ACTIVE_WINDOW;
  wm::WindowState* active_window_state = ash::wm::GetActiveWindowState();
  if (active_window_state) {
    switch (active_window_state->GetStateType()) {
      case wm::WINDOW_STATE_TYPE_MAXIMIZED:
        active_window_state_type = ACTIVE_WINDOW_STATE_TYPE_MAXIMIZED;
        break;
      case wm::WINDOW_STATE_TYPE_FULLSCREEN:
        active_window_state_type = ACTIVE_WINDOW_STATE_TYPE_FULLSCREEN;
        break;
      case wm::WINDOW_STATE_TYPE_LEFT_SNAPPED:
      case wm::WINDOW_STATE_TYPE_RIGHT_SNAPPED:
        active_window_state_type = ACTIVE_WINDOW_STATE_TYPE_SNAPPED;
        break;
      case wm::WINDOW_STATE_TYPE_DOCKED:
      case wm::WINDOW_STATE_TYPE_DOCKED_MINIMIZED:
        active_window_state_type = ACTIVE_WINDOW_STATE_TYPE_DOCKED;
        break;
      case wm::WINDOW_STATE_TYPE_DEFAULT:
      case wm::WINDOW_STATE_TYPE_NORMAL:
      case wm::WINDOW_STATE_TYPE_MINIMIZED:
      case wm::WINDOW_STATE_TYPE_INACTIVE:
      case wm::WINDOW_STATE_TYPE_END:
      case wm::WINDOW_STATE_TYPE_AUTO_POSITIONED:
        active_window_state_type = ACTIVE_WINDOW_STATE_TYPE_OTHER;
        break;
    }
  }
  return active_window_state_type;
}

// Returns true if kiosk mode is active.
bool IsKioskModeActive() {
  return Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus() ==
         user::LOGGED_IN_KIOSK_APP;
}

// Returns true if there is an active user and their session isn't currently
// locked.
bool IsUserActive() {
  switch (Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus()) {
    case user::LOGGED_IN_NONE:
    case user::LOGGED_IN_LOCKED:
      return false;
    case user::LOGGED_IN_USER:
    case user::LOGGED_IN_OWNER:
    case user::LOGGED_IN_GUEST:
    case user::LOGGED_IN_PUBLIC:
    case user::LOGGED_IN_SUPERVISED:
    case user::LOGGED_IN_KIOSK_APP:
      return true;
  }
  NOTREACHED();
  return false;
}

// Array of window container ids that contain visible windows to be counted for
// UMA statistics. Note the containers are ordered from top most visible
// container to the lowest to allow the |GetNumVisibleWindows| method to short
// circuit when processing a maximized or fullscreen window.
int kVisibleWindowContainerIds[] = {kShellWindowId_PanelContainer,
                                    kShellWindowId_DockedContainer,
                                    kShellWindowId_AlwaysOnTopContainer,
                                    kShellWindowId_DefaultContainer};

// Returns an approximate count of how many windows are currently visible in the
// primary root window.
int GetNumVisibleWindowsInPrimaryDisplay() {
  int visible_window_count = 0;
  bool maximized_or_fullscreen_window_present = false;

  for (const int& current_container_id : kVisibleWindowContainerIds) {
    if (maximized_or_fullscreen_window_present)
      break;

    const aura::Window::Windows& children =
        Shell::GetContainer(Shell::GetInstance()->GetPrimaryRootWindow(),
                            current_container_id)->children();
    // Reverse iterate over the child windows so that they are processed in
    // visible stacking order.
    for (aura::Window::Windows::const_reverse_iterator it = children.rbegin(),
                                                       rend = children.rend();
         it != rend; ++it) {
      const aura::Window* child_window = *it;
      const wm::WindowState* child_window_state =
          wm::GetWindowState(child_window);

      if (!child_window->IsVisible() || child_window_state->IsMinimized())
        continue;

      // Only count activatable windows for 2 reasons:
      //  1. Ensures that a browser window and its transient, modal child will
      //     only count as 1 visible window.
      //  2. Prevents counting some windows in the
      //     kShellWindowId_DockedContainer that were not opened by the user.
      if (child_window_state->CanActivate())
        ++visible_window_count;

      // Stop counting windows that will be hidden by maximized or fullscreen
      // windows. Only windows in the kShellWindowId_DefaultContainer and
      // kShellWindowId_AlwaysOnTopContainer can be maximized or fullscreened
      // and completely obscure windows beneath them.
      if ((kShellWindowId_DefaultContainer == current_container_id ||
           kShellWindowId_AlwaysOnTopContainer == current_container_id) &&
          child_window_state->IsMaximizedOrFullscreen()) {
        maximized_or_fullscreen_window_present = true;
        break;
      }
    }
  }
  return visible_window_count;
}

// Records the number of items in the shelf as an UMA statistic.
void RecordShelfItemCounts() {
  ShelfDelegate* shelf_delegate = Shell::GetInstance()->GetShelfDelegate();
  int pinned_item_count = 0;
  int unpinned_item_count = 0;

  for (const ShelfItem& shelf_item :
       Shell::GetInstance()->shelf_model()->items()) {
    if (shelf_item.type != TYPE_APP_LIST) {
      // Internal ash apps do not have an app id and thus will always be counted
      // as unpinned.
      if (shelf_delegate->HasShelfIDToAppIDMapping(shelf_item.id) &&
          shelf_delegate->IsAppPinned(
              shelf_delegate->GetAppIDForShelfID(shelf_item.id))) {
        ++pinned_item_count;
      } else {
        ++unpinned_item_count;
      }
    }
  }

  UMA_HISTOGRAM_COUNTS_100("Ash.Shelf.NumberOfItems",
                           pinned_item_count + unpinned_item_count);
  UMA_HISTOGRAM_COUNTS_100("Ash.Shelf.NumberOfPinnedItems", pinned_item_count);
  UMA_HISTOGRAM_COUNTS_100("Ash.Shelf.NumberOfUnpinnedItems",
                           unpinned_item_count);
}

}  // namespace

UserMetricsRecorder::UserMetricsRecorder() {
  StartTimer();
}

UserMetricsRecorder::UserMetricsRecorder(bool record_periodic_metrics) {
  if (record_periodic_metrics)
    StartTimer();
}

UserMetricsRecorder::~UserMetricsRecorder() {
  timer_.Stop();
}

void UserMetricsRecorder::RecordUserMetricsAction(UserMetricsAction action) {
  switch (action) {
    case ash::UMA_ACCEL_KEYBOARD_BRIGHTNESS_DOWN_F6:
      base::RecordAction(
          base::UserMetricsAction("Accel_KeyboardBrightnessDown_F6"));
      break;
    case ash::UMA_ACCEL_KEYBOARD_BRIGHTNESS_UP_F7:
      base::RecordAction(
          base::UserMetricsAction("Accel_KeyboardBrightnessUp_F7"));
      break;
    case ash::UMA_ACCEL_LOCK_SCREEN_LOCK_BUTTON:
      base::RecordAction(
          base::UserMetricsAction("Accel_LockScreen_LockButton"));
      break;
    case ash::UMA_ACCEL_LOCK_SCREEN_POWER_BUTTON:
      base::RecordAction(
          base::UserMetricsAction("Accel_LockScreen_PowerButton"));
      break;
    case ash::UMA_ACCEL_MAXIMIZE_RESTORE_F4:
      base::RecordAction(
          base::UserMetricsAction("Accel_Maximize_Restore_F4"));
      break;
    case ash::UMA_ACCEL_PREVWINDOW_F5:
      base::RecordAction(base::UserMetricsAction("Accel_PrevWindow_F5"));
      break;
    case ash::UMA_ACCEL_EXIT_FIRST_Q:
      base::RecordAction(base::UserMetricsAction("Accel_Exit_First_Q"));
      break;
    case ash::UMA_ACCEL_EXIT_SECOND_Q:
      base::RecordAction(base::UserMetricsAction("Accel_Exit_Second_Q"));
      break;
    case ash::UMA_ACCEL_RESTART_POWER_BUTTON:
      base::RecordAction(
          base::UserMetricsAction("Accel_Restart_PowerButton"));
      break;
    case ash::UMA_ACCEL_SHUT_DOWN_POWER_BUTTON:
      base::RecordAction(
          base::UserMetricsAction("Accel_ShutDown_PowerButton"));
      break;
    case ash::UMA_CLOSE_THROUGH_CONTEXT_MENU:
      base::RecordAction(base::UserMetricsAction("CloseFromContextMenu"));
      break;
    case ash::UMA_DESKTOP_SWITCH_TASK:
      base::RecordAction(base::UserMetricsAction("Desktop_SwitchTask"));
      task_switch_metrics_recorder_.OnTaskSwitch(
          TaskSwitchMetricsRecorder::DESKTOP);
      break;
    case ash::UMA_DRAG_MAXIMIZE_LEFT:
      base::RecordAction(base::UserMetricsAction("WindowDrag_MaximizeLeft"));
      break;
    case ash::UMA_DRAG_MAXIMIZE_RIGHT:
      base::RecordAction(base::UserMetricsAction("WindowDrag_MaximizeRight"));
      break;
    case ash::UMA_LAUNCHER_BUTTON_PRESSED_WITH_MOUSE:
      base::RecordAction(
          base::UserMetricsAction("Launcher_ButtonPressed_Mouse"));
      break;
    case ash::UMA_LAUNCHER_BUTTON_PRESSED_WITH_TOUCH:
      base::RecordAction(
          base::UserMetricsAction("Launcher_ButtonPressed_Touch"));
      break;
    case ash::UMA_LAUNCHER_CLICK_ON_APP:
      base::RecordAction(base::UserMetricsAction("Launcher_ClickOnApp"));
      break;
    case ash::UMA_LAUNCHER_CLICK_ON_APPLIST_BUTTON:
      base::RecordAction(
          base::UserMetricsAction("Launcher_ClickOnApplistButton"));
      break;
    case ash::UMA_LAUNCHER_LAUNCH_TASK:
      base::RecordAction(base::UserMetricsAction("Launcher_LaunchTask"));
      task_switch_metrics_recorder_.OnTaskSwitch(
          TaskSwitchMetricsRecorder::SHELF);
      break;
    case ash::UMA_LAUNCHER_MINIMIZE_TASK:
      base::RecordAction(base::UserMetricsAction("Launcher_MinimizeTask"));
      break;
    case ash::UMA_LAUNCHER_SWITCH_TASK:
      base::RecordAction(base::UserMetricsAction("Launcher_SwitchTask"));
      task_switch_metrics_recorder_.OnTaskSwitch(
          TaskSwitchMetricsRecorder::SHELF);
      break;
    case UMA_MAXIMIZE_MODE_DISABLED:
      base::RecordAction(base::UserMetricsAction("Touchview_Disabled"));
      break;
    case UMA_MAXIMIZE_MODE_ENABLED:
      base::RecordAction(base::UserMetricsAction("Touchview_Enabled"));
      break;
    case UMA_MAXIMIZE_MODE_INITIALLY_DISABLED:
      base::RecordAction(
          base::UserMetricsAction("Touchview_Initially_Disabled"));
      break;
    case ash::UMA_MOUSE_DOWN:
      base::RecordAction(base::UserMetricsAction("Mouse_Down"));
      break;
    case ash::UMA_PANEL_MINIMIZE_CAPTION_CLICK:
      base::RecordAction(
         base::UserMetricsAction("Panel_Minimize_Caption_Click"));
      break;
    case ash::UMA_PANEL_MINIMIZE_CAPTION_GESTURE:
      base::RecordAction(
          base::UserMetricsAction("Panel_Minimize_Caption_Gesture"));
      break;
    case ash::UMA_SHELF_ALIGNMENT_SET_BOTTOM:
      base::RecordAction(
          base::UserMetricsAction("Shelf_AlignmentSetBottom"));
      break;
    case ash::UMA_SHELF_ALIGNMENT_SET_LEFT:
      base::RecordAction(
          base::UserMetricsAction("Shelf_AlignmentSetLeft"));
      break;
    case ash::UMA_SHELF_ALIGNMENT_SET_RIGHT:
      base::RecordAction(
          base::UserMetricsAction("Shelf_AlignmentSetRight"));
      break;
    case ash::UMA_STATUS_AREA_AUDIO_CURRENT_INPUT_DEVICE:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Audio_CurrentInputDevice"));
      break;
    case ash::UMA_STATUS_AREA_AUDIO_CURRENT_OUTPUT_DEVICE:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Audio_CurrentOutputDevice"));
      break;
    case ash::UMA_STATUS_AREA_AUDIO_SWITCH_INPUT_DEVICE:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Audio_SwitchInputDevice"));
      break;
    case ash::UMA_STATUS_AREA_AUDIO_SWITCH_OUTPUT_DEVICE:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Audio_SwitchOutputDevice"));
      break;
    case ash::UMA_STATUS_AREA_BRIGHTNESS_CHANGED:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_BrightnessChanged"));
      break;
    case ash::UMA_STATUS_AREA_BLUETOOTH_CONNECT_KNOWN_DEVICE:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Bluetooth_Connect_Known"));
      break;
    case ash::UMA_STATUS_AREA_BLUETOOTH_CONNECT_UNKNOWN_DEVICE:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Bluetooth_Connect_Unknown"));
      break;
    case ash::UMA_STATUS_AREA_BLUETOOTH_DISABLED:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Bluetooth_Disabled"));
      break;
    case ash::UMA_STATUS_AREA_BLUETOOTH_ENABLED:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Bluetooth_Enabled"));
      break;
    case ash::UMA_STATUS_AREA_CAPS_LOCK_DETAILED:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_CapsLock_Detailed"));
      break;
    case ash::UMA_STATUS_AREA_CAPS_LOCK_DISABLED_BY_CLICK:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_CapsLock_DisabledByClick"));
      break;
    case ash::UMA_STATUS_AREA_CAPS_LOCK_ENABLED_BY_CLICK:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_CapsLock_EnabledByClick"));
      break;
    case ash::UMA_STATUS_AREA_CAPS_LOCK_POPUP:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_CapsLock_Popup"));
      break;
    case ash::UMA_STATUS_AREA_CAST_STOP_CAST:
      base::RecordAction(base::UserMetricsAction("StatusArea_Cast_StopCast"));
      break;
    case ash::UMA_STATUS_AREA_CONNECT_TO_CONFIGURED_NETWORK:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Network_ConnectConfigured"));
      break;
    case ash::UMA_STATUS_AREA_CONNECT_TO_UNCONFIGURED_NETWORK:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Network_ConnectUnconfigured"));
      break;
    case ash::UMA_STATUS_AREA_CONNECT_TO_VPN:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_VPN_ConnectToNetwork"));
      break;
    case ash::UMA_STATUS_AREA_CHANGED_VOLUME_MENU:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Volume_ChangedMenu"));
      break;
    case ash::UMA_STATUS_AREA_CHANGED_VOLUME_POPUP:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Volume_ChangedPopup"));
      break;
    case ash::UMA_STATUS_AREA_DETAILED_ACCESSABILITY:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Accessability_DetailedView"));
      break;
    case ash::UMA_STATUS_AREA_DETAILED_AUDIO_VIEW:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Audio_Detailed"));
      break;
    case ash::UMA_STATUS_AREA_DETAILED_BLUETOOTH_VIEW:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Bluetooth_Detailed"));
      break;
    case ash::UMA_STATUS_AREA_DETAILED_BRIGHTNESS_VIEW:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Brightness_Detailed"));
      break;
    case ash::UMA_STATUS_AREA_DETAILED_CAST_VIEW:
      base::RecordAction(base::UserMetricsAction("StatusArea_Cast_Detailed"));
      break;
    case ash::UMA_STATUS_AREA_DETAILED_CAST_VIEW_LAUNCH_CAST:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Cast_Detailed_Launch_Cast"));
      break;
    case ash::UMA_STATUS_AREA_DETAILED_DRIVE_VIEW:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Drive_Detailed"));
      break;
    case ash::UMA_STATUS_AREA_DETAILED_NETWORK_VIEW:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Network_Detailed"));
      break;
    case ash::UMA_STATUS_AREA_DETAILED_VPN_VIEW:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_VPN_Detailed"));
      break;
    case ash::UMA_STATUS_AREA_DISABLE_AUTO_CLICK:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_AutoClickDisabled"));
      break;
    case ash::UMA_STATUS_AREA_DISABLE_HIGH_CONTRAST:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_HighContrastDisabled"));
      break;
    case ash::UMA_STATUS_AREA_DISABLE_LARGE_CURSOR:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_LargeCursorDisabled"));
      break;
    case ash::UMA_STATUS_AREA_DISABLE_MAGNIFIER:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_MagnifierDisabled"));
      break;
    case ash::UMA_STATUS_AREA_DISABLE_SPOKEN_FEEDBACK:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_SpokenFeedbackDisabled"));
      break;
    case ash::UMA_STATUS_AREA_DISABLE_VIRTUAL_KEYBOARD:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_VirtualKeyboardDisabled"));
      break;
    case ash::UMA_STATUS_AREA_DISABLE_WIFI:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Network_WifiDisabled"));
      break;
    case ash::UMA_STATUS_AREA_DRIVE_CANCEL_OPERATION:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Drive_CancelOperation"));
      break;
    case ash::UMA_STATUS_AREA_DRIVE_SETTINGS:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Drive_Settings"));
      break;
    case ash::UMA_STATUS_AREA_ENABLE_AUTO_CLICK:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_AutoClickEnabled"));
      break;
    case ash::UMA_STATUS_AREA_ENABLE_HIGH_CONTRAST:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_HighContrastEnabled"));
      break;
    case ash::UMA_STATUS_AREA_ENABLE_LARGE_CURSOR:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_LargeCursorEnabled"));
      break;
    case ash::UMA_STATUS_AREA_ENABLE_MAGNIFIER:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_MagnifierEnabled"));
      break;
    case ash::UMA_STATUS_AREA_ENABLE_SPOKEN_FEEDBACK:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_SpokenFeedbackEnabled"));
      break;
    case ash::UMA_STATUS_AREA_ENABLE_VIRTUAL_KEYBOARD:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_VirtualKeyboardEnabled"));
      break;
    case ash::UMA_STATUS_AREA_ENABLE_WIFI:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Network_WifiEnabled"));
      break;
    case ash::UMA_STATUS_AREA_IME_SHOW_DETAILED:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_IME_Detailed"));
      break;
    case ash::UMA_STATUS_AREA_IME_SWITCH_MODE:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_IME_SwitchMode"));
      break;
    case ash::UMA_STATUS_AREA_MENU_OPENED:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_MenuOpened"));
      break;
    case ash::UMA_STATUS_AREA_NETWORK_JOIN_OTHER_CLICKED:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Network_JoinOther"));
      break;
    case ash::UMA_STATUS_AREA_NETWORK_SETTINGS_CLICKED:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Network_Settings"));
      break;
    case ash::UMA_STATUS_AREA_SHOW_NETWORK_CONNECTION_DETAILS:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_Network_ConnectionDetails"));
      break;
    case ash::UMA_STATUS_AREA_SHOW_VPN_CONNECTION_DETAILS:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_VPN_ConnectionDetails"));
      break;
    case ash::UMA_STATUS_AREA_SIGN_OUT:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_SignOut"));
      break;
    case ash::UMA_STATUS_AREA_VPN_ADD_BUILT_IN_CLICKED:
      base::RecordAction(base::UserMetricsAction("StatusArea_VPN_AddBuiltIn"));
      break;
    case ash::UMA_STATUS_AREA_VPN_ADD_THIRD_PARTY_CLICKED:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_VPN_AddThirdParty"));
      break;
    case ash::UMA_STATUS_AREA_VPN_DISCONNECT_CLICKED:
      base::RecordAction(base::UserMetricsAction("StatusArea_VPN_Disconnect"));
      break;
    case ash::UMA_STATUS_AREA_VPN_SETTINGS_CLICKED:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_VPN_Settings"));
      break;
    case ash::UMA_TOGGLE_MAXIMIZE_CAPTION_CLICK:
      base::RecordAction(
          base::UserMetricsAction("Caption_ClickTogglesMaximize"));
      break;
    case ash::UMA_TOGGLE_MAXIMIZE_CAPTION_GESTURE:
      base::RecordAction(
          base::UserMetricsAction("Caption_GestureTogglesMaximize"));
      break;
    case ash::UMA_TOGGLE_SINGLE_AXIS_MAXIMIZE_BORDER_CLICK:
      base::RecordAction(
          base::UserMetricsAction(
              "WindowBorder_ClickTogglesSingleAxisMaximize"));
      break;
    case ash::UMA_TOUCHPAD_GESTURE_OVERVIEW:
      base::RecordAction(
          base::UserMetricsAction("Touchpad_Gesture_Overview"));
      break;
    case ash::UMA_TOUCHSCREEN_TAP_DOWN:
      base::RecordAction(base::UserMetricsAction("Touchscreen_Down"));
      break;
    case ash::UMA_TRAY_HELP:
      base::RecordAction(base::UserMetricsAction("Tray_Help"));
      break;
    case ash::UMA_TRAY_LOCK_SCREEN:
      base::RecordAction(base::UserMetricsAction("Tray_LockScreen"));
      break;
    case ash::UMA_TRAY_OVERVIEW:
      base::RecordAction(base::UserMetricsAction("Tray_Overview"));
      break;
    case ash::UMA_TRAY_SHUT_DOWN:
      base::RecordAction(base::UserMetricsAction("Tray_ShutDown"));
      break;
    case ash::UMA_WINDOW_APP_CLOSE_BUTTON_CLICK:
      base::RecordAction(base::UserMetricsAction("AppCloseButton_Clk"));
      break;
    case ash::UMA_WINDOW_CLOSE_BUTTON_CLICK:
      base::RecordAction(base::UserMetricsAction("CloseButton_Clk"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_EXIT_FULLSCREEN:
      base::RecordAction(base::UserMetricsAction("MaxButton_Clk_ExitFS"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_RESTORE:
      base::RecordAction(
          base::UserMetricsAction("MaxButton_Clk_Restore"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_MAXIMIZE:
      base::RecordAction(
          base::UserMetricsAction("MaxButton_Clk_Maximize"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_MINIMIZE:
      base::RecordAction(base::UserMetricsAction("MinButton_Clk"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_LEFT:
      base::RecordAction(base::UserMetricsAction("MaxButton_MaxLeft"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_RIGHT:
      base::RecordAction(base::UserMetricsAction("MaxButton_MaxRight"));
      break;
    case ash::UMA_WINDOW_OVERVIEW:
      base::RecordAction(
          base::UserMetricsAction("WindowSelector_Overview"));
      break;
    case ash::UMA_WINDOW_OVERVIEW_ACTIVE_WINDOW_CHANGED:
      base::RecordAction(
          base::UserMetricsAction("WindowSelector_ActiveWindowChanged"));
      task_switch_metrics_recorder_.OnTaskSwitch(
          TaskSwitchMetricsRecorder::OVERVIEW_MODE);
      break;
    case ash::UMA_WINDOW_OVERVIEW_ENTER_KEY:
      base::RecordAction(
          base::UserMetricsAction("WindowSelector_OverviewEnterKey"));
      break;
    case ash::UMA_WINDOW_CYCLE:
      base::RecordAction(
          base::UserMetricsAction("WindowCycleController_Cycle"));
      break;
  }
}

void UserMetricsRecorder::OnShellInitialized() {
  // Lazy creation of the DesktopTaskSwitchMetricRecorder because it accesses
  // Shell::GetInstance() which is not available when |this| is instantiated.
  if (!desktop_task_switch_metric_recorder_) {
    desktop_task_switch_metric_recorder_.reset(
        new DesktopTaskSwitchMetricRecorder());
  }
}

void UserMetricsRecorder::OnShellShuttingDown() {
  desktop_task_switch_metric_recorder_.reset();
}

void UserMetricsRecorder::RecordPeriodicMetrics() {
  Shelf* shelf = Shelf::ForPrimaryDisplay();
  // TODO(bruthig): Investigating whether the check for |manager| is necessary
  // and add tests if it is.
  if (shelf) {
    // TODO(bruthig): Consider tracking the time spent in each alignment.
    UMA_HISTOGRAM_ENUMERATION("Ash.ShelfAlignmentOverTime",
                              shelf->SelectValueForShelfAlignment(
                                  SHELF_ALIGNMENT_UMA_ENUM_VALUE_BOTTOM,
                                  SHELF_ALIGNMENT_UMA_ENUM_VALUE_LEFT,
                                  SHELF_ALIGNMENT_UMA_ENUM_VALUE_RIGHT),
                              SHELF_ALIGNMENT_UMA_ENUM_VALUE_COUNT);
  }

  if (IsUserInActiveDesktopEnvironment()) {
    RecordShelfItemCounts();
    UMA_HISTOGRAM_COUNTS_100("Ash.NumberOfVisibleWindowsInPrimaryDisplay",
                             GetNumVisibleWindowsInPrimaryDisplay());
  }

  // TODO(bruthig): Find out if this should only be logged when the user is
  // active.
  // TODO(bruthig): Consider tracking how long a particular type of window is
  // active at a time.
  UMA_HISTOGRAM_ENUMERATION("Ash.ActiveWindowShowTypeOverTime",
                            GetActiveWindowState(),
                            ACTIVE_WINDOW_STATE_TYPE_COUNT);
}

bool UserMetricsRecorder::IsUserInActiveDesktopEnvironment() const {
  return IsUserActive() && !IsKioskModeActive();
}

void UserMetricsRecorder::StartTimer() {
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(kAshPeriodicMetricsTimeInSeconds),
               this, &UserMetricsRecorder::RecordPeriodicMetrics);
}

}  // namespace ash
