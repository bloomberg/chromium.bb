// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/user_metrics_recorder.h"

#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "base/metrics/histogram.h"
#include "base/metrics/user_metrics.h"

namespace ash {

// Time in seconds between calls to "RecordPeriodicMetrics".
const int kAshPeriodicMetricsTimeInSeconds = 30 * 60;

UserMetricsRecorder::UserMetricsRecorder() {
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromSeconds(kAshPeriodicMetricsTimeInSeconds),
               this,
               &UserMetricsRecorder::RecordPeriodicMetrics);
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
    case ash::UMA_ACCEL_SHUT_DOWN_POWER_BUTTON:
      base::RecordAction(
          base::UserMetricsAction("Accel_ShutDown_PowerButton"));
      break;
    case ash::UMA_CLOSE_THROUGH_CONTEXT_MENU:
      base::RecordAction(base::UserMetricsAction("CloseFromContextMenu"));
      break;
    case ash::UMA_DRAG_MAXIMIZE_LEFT:
      base::RecordAction(base::UserMetricsAction("WindowDrag_MaximizeLeft"));
      break;
    case ash::UMA_DRAG_MAXIMIZE_RIGHT:
      base::RecordAction(base::UserMetricsAction("WindowDrag_MaximizeRight"));
      break;
    case ash::UMA_GESTURE_OVERVIEW:
      base::RecordAction(base::UserMetricsAction("Gesture_Overview"));
      break;
    case ash::UMA_LAUNCHER_CLICK_ON_APP:
      base::RecordAction(base::UserMetricsAction("Launcher_ClickOnApp"));
      break;
    case ash::UMA_LAUNCHER_CLICK_ON_APPLIST_BUTTON:
      base::RecordAction(
          base::UserMetricsAction("Launcher_ClickOnApplistButton"));
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
    case ash::UMA_STATUS_AREA_VPN_JOIN_OTHER_CLICKED:
      base::RecordAction(
          base::UserMetricsAction("StatusArea_VPN_JoinOther"));
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

void UserMetricsRecorder::RecordPeriodicMetrics() {
  ShelfLayoutManager* manager =
      ShelfLayoutManager::ForShelf(Shell::GetPrimaryRootWindow());
  if (manager) {
    UMA_HISTOGRAM_ENUMERATION("Ash.ShelfAlignmentOverTime",
                              manager->SelectValueForShelfAlignment(
                                  SHELF_ALIGNMENT_UMA_ENUM_VALUE_BOTTOM,
                                  SHELF_ALIGNMENT_UMA_ENUM_VALUE_LEFT,
                                  SHELF_ALIGNMENT_UMA_ENUM_VALUE_RIGHT,
                                  -1),
                              SHELF_ALIGNMENT_UMA_ENUM_VALUE_COUNT);
  }

  enum ActiveWindowStateType {
    ACTIVE_WINDOW_STATE_TYPE_NO_ACTIVE_WINDOW,
    ACTIVE_WINDOW_STATE_TYPE_OTHER,
    ACTIVE_WINDOW_STATE_TYPE_MAXIMIZED,
    ACTIVE_WINDOW_STATE_TYPE_FULLSCREEN,
    ACTIVE_WINDOW_STATE_TYPE_SNAPPED,
    ACTIVE_WINDOW_STATE_TYPE_COUNT
  };

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
      case wm::WINDOW_STATE_TYPE_DEFAULT:
      case wm::WINDOW_STATE_TYPE_NORMAL:
      case wm::WINDOW_STATE_TYPE_MINIMIZED:
      case wm::WINDOW_STATE_TYPE_INACTIVE:
      case wm::WINDOW_STATE_TYPE_DETACHED:
      case wm::WINDOW_STATE_TYPE_END:
      case wm::WINDOW_STATE_TYPE_AUTO_POSITIONED:
        active_window_state_type = ACTIVE_WINDOW_STATE_TYPE_OTHER;
        break;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Ash.ActiveWindowShowTypeOverTime",
                            active_window_state_type,
                            ACTIVE_WINDOW_STATE_TYPE_COUNT);
}

}  // namespace ash
