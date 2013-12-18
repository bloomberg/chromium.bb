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
#include "content/public/browser/user_metrics.h"

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
      content::RecordAction(
          content::UserMetricsAction("Accel_KeyboardBrightnessDown_F6"));
      break;
    case ash::UMA_ACCEL_KEYBOARD_BRIGHTNESS_UP_F7:
      content::RecordAction(
          content::UserMetricsAction("Accel_KeyboardBrightnessUp_F7"));
      break;
    case ash::UMA_ACCEL_LOCK_SCREEN_LOCK_BUTTON:
      content::RecordAction(
          content::UserMetricsAction("Accel_LockScreen_LockButton"));
      break;
    case ash::UMA_ACCEL_LOCK_SCREEN_POWER_BUTTON:
      content::RecordAction(
          content::UserMetricsAction("Accel_LockScreen_PowerButton"));
      break;
    case ash::UMA_ACCEL_MAXIMIZE_RESTORE_F4:
      content::RecordAction(
          content::UserMetricsAction("Accel_Maximize_Restore_F4"));
      break;
    case ash::UMA_ACCEL_PREVWINDOW_F5:
      content::RecordAction(content::UserMetricsAction("Accel_PrevWindow_F5"));
      break;
    case ash::UMA_ACCEL_EXIT_FIRST_Q:
      content::RecordAction(content::UserMetricsAction("Accel_Exit_First_Q"));
      break;
    case ash::UMA_ACCEL_EXIT_SECOND_Q:
      content::RecordAction(content::UserMetricsAction("Accel_Exit_Second_Q"));
      break;
    case ash::UMA_ACCEL_SHUT_DOWN_POWER_BUTTON:
      content::RecordAction(
          content::UserMetricsAction("Accel_ShutDown_PowerButton"));
      break;
    case ash::UMA_CLOSE_THROUGH_CONTEXT_MENU:
      content::RecordAction(content::UserMetricsAction("CloseFromContextMenu"));
      break;
    case ash::UMA_GESTURE_OVERVIEW:
      content::RecordAction(content::UserMetricsAction("Gesture_Overview"));
      break;
    case ash::UMA_LAUNCHER_CLICK_ON_APP:
      content::RecordAction(content::UserMetricsAction("Launcher_ClickOnApp"));
      break;
    case ash::UMA_LAUNCHER_CLICK_ON_APPLIST_BUTTON:
      content::RecordAction(
          content::UserMetricsAction("Launcher_ClickOnApplistButton"));
      break;
    case ash::UMA_MOUSE_DOWN:
      content::RecordAction(content::UserMetricsAction("Mouse_Down"));
      break;
    case ash::UMA_SHELF_ALIGNMENT_SET_BOTTOM:
      content::RecordAction(
          content::UserMetricsAction("Shelf_AlignmentSetBottom"));
      break;
    case ash::UMA_SHELF_ALIGNMENT_SET_LEFT:
      content::RecordAction(
          content::UserMetricsAction("Shelf_AlignmentSetLeft"));
      break;
    case ash::UMA_SHELF_ALIGNMENT_SET_RIGHT:
      content::RecordAction(
          content::UserMetricsAction("Shelf_AlignmentSetRight"));
    case ash::UMA_STATUS_AREA_AUDIO_CURRENT_INPUT_DEVICE:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Audio_CurrentInputDevice"));
      break;
    case ash::UMA_STATUS_AREA_AUDIO_CURRENT_OUTPUT_DEVICE:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Audio_CurrentOutputDevice"));
      break;
    case ash::UMA_STATUS_AREA_AUDIO_SWITCH_INPUT_DEVICE:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Audio_SwitchInputDevice"));
      break;
    case ash::UMA_STATUS_AREA_AUDIO_SWITCH_OUTPUT_DEVICE:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Audio_SwitchOutputDevice"));
      break;
    case ash::UMA_STATUS_AREA_BRIGHTNESS_CHANGED:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_BrightnessChanged"));
      break;
    case ash::UMA_STATUS_AREA_BLUETOOTH_CONNECT_KNOWN_DEVICE:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Bluetooth_Connect_Known"));
      break;
    case ash::UMA_STATUS_AREA_BLUETOOTH_CONNECT_UNKNOWN_DEVICE:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Bluetooth_Connect_Unknown"));
      break;
    case ash::UMA_STATUS_AREA_BLUETOOTH_DISABLED:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Bluetooth_Disabled"));
      break;
    case ash::UMA_STATUS_AREA_BLUETOOTH_ENABLED:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Bluetooth_Enabled"));
      break;
    case ash::UMA_STATUS_AREA_CAPS_LOCK_DETAILED:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_CapsLock_Detailed"));
      break;
    case ash::UMA_STATUS_AREA_CAPS_LOCK_DISABLED_BY_CLICK:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_CapsLock_DisabledByClick"));
      break;
    case ash::UMA_STATUS_AREA_CAPS_LOCK_ENABLED_BY_CLICK:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_CapsLock_EnabledByClick"));
      break;
    case ash::UMA_STATUS_AREA_CAPS_LOCK_POPUP:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_CapsLock_Popup"));
      break;
    case ash::UMA_STATUS_AREA_CONNECT_TO_CONFIGURED_NETWORK:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Network_ConnectConfigured"));
      break;
    case ash::UMA_STATUS_AREA_CONNECT_TO_UNCONFIGURED_NETWORK:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Network_ConnectUnconfigured"));
      break;
    case ash::UMA_STATUS_AREA_CONNECT_TO_VPN:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_VPN_ConnectToNetwork"));
      break;
    case ash::UMA_STATUS_AREA_CHANGED_VOLUME_MENU:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Volume_ChangedMenu"));
      break;
    case ash::UMA_STATUS_AREA_CHANGED_VOLUME_POPUP:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Volume_ChangedPopup"));
      break;
    case ash::UMA_STATUS_AREA_DETAILED_ACCESSABILITY:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Accessability_DetailedView"));
      break;
    case ash::UMA_STATUS_AREA_DETAILED_AUDIO_VIEW:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Audio_Detailed"));
      break;
    case ash::UMA_STATUS_AREA_DETAILED_BLUETOOTH_VIEW:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Bluetooth_Detailed"));
      break;
    case ash::UMA_STATUS_AREA_DETAILED_BRIGHTNESS_VIEW:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Brightness_Detailed"));
      break;
    case ash::UMA_STATUS_AREA_DETAILED_DRIVE_VIEW:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Drive_Detailed"));
      break;
    case ash::UMA_STATUS_AREA_DETAILED_NETWORK_VIEW:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Network_Detailed"));
      break;
    case ash::UMA_STATUS_AREA_DETAILED_VPN_VIEW:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_VPN_Detailed"));
      break;
    case ash::UMA_STATUS_AREA_DISABLE_AUTO_CLICK:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_AutoClickDisabled"));
      break;
    case ash::UMA_STATUS_AREA_DISABLE_HIGH_CONTRAST:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_HighContrastDisabled"));
      break;
    case ash::UMA_STATUS_AREA_DISABLE_LARGE_CURSOR:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_LargeCursorDisabled"));
      break;
    case ash::UMA_STATUS_AREA_DISABLE_MAGNIFIER:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_MagnifierDisabled"));
      break;
    case ash::UMA_STATUS_AREA_DISABLE_SPOKEN_FEEDBACK:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_SpokenFeedbackDisabled"));
      break;
    case ash::UMA_STATUS_AREA_DISABLE_WIFI:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Network_WifiDisabled"));
      break;
    case ash::UMA_STATUS_AREA_DRIVE_CANCEL_OPERATION:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Drive_CancelOperation"));
      break;
    case ash::UMA_STATUS_AREA_DRIVE_SETTINGS:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Drive_Settings"));
      break;
    case ash::UMA_STATUS_AREA_ENABLE_AUTO_CLICK:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_AutoClickEnabled"));
      break;
    case ash::UMA_STATUS_AREA_ENABLE_HIGH_CONTRAST:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_HighContrastEnabled"));
      break;
    case ash::UMA_STATUS_AREA_ENABLE_LARGE_CURSOR:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_LargeCursorEnabled"));
      break;
    case ash::UMA_STATUS_AREA_ENABLE_MAGNIFIER:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_MagnifierEnabled"));
      break;
    case ash::UMA_STATUS_AREA_ENABLE_SPOKEN_FEEDBACK:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_SpokenFeedbackEnabled"));
      break;
    case ash::UMA_STATUS_AREA_ENABLE_WIFI:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Network_WifiEnabled"));
      break;
    case ash::UMA_STATUS_AREA_IME_SHOW_DETAILED:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_IME_Detailed"));
      break;
    case ash::UMA_STATUS_AREA_IME_SWITCH_MODE:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_IME_SwitchMode"));
      break;
    case ash::UMA_STATUS_AREA_MENU_OPENED:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_MenuOpened"));
      break;
    case ash::UMA_STATUS_AREA_NETWORK_JOIN_OTHER_CLICKED:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Network_JoinOther"));
      break;
    case ash::UMA_STATUS_AREA_NETWORK_SETTINGS_CLICKED:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Network_Settings"));
      break;
    case ash::UMA_STATUS_AREA_SHOW_NETWORK_CONNECTION_DETAILS:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_Network_ConnectionDetails"));
      break;
    case ash::UMA_STATUS_AREA_SHOW_VPN_CONNECTION_DETAILS:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_VPN_ConnectionDetails"));
      break;
    case ash::UMA_STATUS_AREA_SIGN_OUT:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_SignOut"));
      break;
    case ash::UMA_STATUS_AREA_VPN_JOIN_OTHER_CLICKED:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_VPN_JoinOther"));
      break;
    case ash::UMA_STATUS_AREA_VPN_SETTINGS_CLICKED:
      content::RecordAction(
          content::UserMetricsAction("StatusArea_VPN_Settings"));
      break;
    case ash::UMA_TOGGLE_MAXIMIZE_CAPTION_CLICK:
      content::RecordAction(
          content::UserMetricsAction("Caption_ClickTogglesMaximize"));
      break;
    case ash::UMA_TOGGLE_MAXIMIZE_CAPTION_GESTURE:
      content::RecordAction(
          content::UserMetricsAction("Caption_GestureTogglesMaximize"));
      break;
    case ash::UMA_TOUCHPAD_GESTURE_OVERVIEW:
      content::RecordAction(
          content::UserMetricsAction("Touchpad_Gesture_Overview"));
      break;
    case ash::UMA_TOUCHSCREEN_TAP_DOWN:
      content::RecordAction(content::UserMetricsAction("Touchscreen_Down"));
      break;
    case ash::UMA_TRAY_HELP:
      content::RecordAction(content::UserMetricsAction("Tray_Help"));
      break;
    case ash::UMA_TRAY_LOCK_SCREEN:
      content::RecordAction(content::UserMetricsAction("Tray_LockScreen"));
      break;
    case ash::UMA_TRAY_SHUT_DOWN:
      content::RecordAction(content::UserMetricsAction("Tray_ShutDown"));
      break;
    case ash::UMA_WINDOW_APP_CLOSE_BUTTON_CLICK:
      content::RecordAction(content::UserMetricsAction("AppCloseButton_Clk"));
      break;
    case ash::UMA_WINDOW_CLOSE_BUTTON_CLICK:
      content::RecordAction(content::UserMetricsAction("CloseButton_Clk"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_EXIT_FULLSCREEN:
      content::RecordAction(content::UserMetricsAction("MaxButton_Clk_ExitFS"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_RESTORE:
      content::RecordAction(
          content::UserMetricsAction("MaxButton_Clk_Restore"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_MAXIMIZE:
      content::RecordAction(
          content::UserMetricsAction("MaxButton_Clk_Maximize"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_CLICK_MINIMIZE:
      content::RecordAction(content::UserMetricsAction("MinButton_Clk"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE:
      content::RecordAction(content::UserMetricsAction("MaxButton_Maximize"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_LEFT:
      content::RecordAction(content::UserMetricsAction("MaxButton_MaxLeft"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_RIGHT:
      content::RecordAction(content::UserMetricsAction("MaxButton_MaxRight"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_MINIMIZE:
      content::RecordAction(content::UserMetricsAction("MaxButton_Minimize"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_RESTORE:
      content::RecordAction(content::UserMetricsAction("MaxButton_Restore"));
      break;
    case ash::UMA_WINDOW_MAXIMIZE_BUTTON_SHOW_BUBBLE:
      content::RecordAction(content::UserMetricsAction("MaxButton_ShowBubble"));
      break;
    case ash::UMA_WINDOW_OVERVIEW:
      content::RecordAction(
          content::UserMetricsAction("WindowSelector_Overview"));
      break;
    case ash::UMA_WINDOW_SELECTION:
      content::RecordAction(
          content::UserMetricsAction("WindowSelector_Selection"));
      break;
  }
}

void UserMetricsRecorder::RecordPeriodicMetrics() {
  internal::ShelfLayoutManager* manager =
      internal::ShelfLayoutManager::ForLauncher(Shell::GetPrimaryRootWindow());
  if (manager) {
    UMA_HISTOGRAM_ENUMERATION("Ash.ShelfAlignmentOverTime",
        manager->SelectValueForShelfAlignment(
            internal::SHELF_ALIGNMENT_UMA_ENUM_VALUE_BOTTOM,
            internal::SHELF_ALIGNMENT_UMA_ENUM_VALUE_LEFT,
            internal::SHELF_ALIGNMENT_UMA_ENUM_VALUE_RIGHT,
            -1),
        internal::SHELF_ALIGNMENT_UMA_ENUM_VALUE_COUNT);
  }

  enum ActiveWindowShowType {
    ACTIVE_WINDOW_SHOW_TYPE_NO_ACTIVE_WINDOW,
    ACTIVE_WINDOW_SHOW_TYPE_OTHER,
    ACTIVE_WINDOW_SHOW_TYPE_MAXIMIZED,
    ACTIVE_WINDOW_SHOW_TYPE_FULLSCREEN,
    ACTIVE_WINDOW_SHOW_TYPE_SNAPPED,
    ACTIVE_WINDOW_SHOW_TYPE_COUNT
  };

  ActiveWindowShowType active_window_show_type =
      ACTIVE_WINDOW_SHOW_TYPE_NO_ACTIVE_WINDOW;
  wm::WindowState* active_window_state = ash::wm::GetActiveWindowState();
  if (active_window_state) {
    switch(active_window_state->window_show_type()) {
      case wm::SHOW_TYPE_MAXIMIZED:
        active_window_show_type = ACTIVE_WINDOW_SHOW_TYPE_MAXIMIZED;
        break;
      case wm::SHOW_TYPE_FULLSCREEN:
        active_window_show_type = ACTIVE_WINDOW_SHOW_TYPE_FULLSCREEN;
        break;
      case wm::SHOW_TYPE_LEFT_SNAPPED:
      case wm::SHOW_TYPE_RIGHT_SNAPPED:
        active_window_show_type = ACTIVE_WINDOW_SHOW_TYPE_SNAPPED;
        break;
      case wm::SHOW_TYPE_DEFAULT:
      case wm::SHOW_TYPE_NORMAL:
      case wm::SHOW_TYPE_MINIMIZED:
      case wm::SHOW_TYPE_INACTIVE:
      case wm::SHOW_TYPE_DETACHED:
      case wm::SHOW_TYPE_END:
      case wm::SHOW_TYPE_AUTO_POSITIONED:
        active_window_show_type = ACTIVE_WINDOW_SHOW_TYPE_OTHER;
        break;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Ash.ActiveWindowShowTypeOverTime",
                            active_window_show_type,
                            ACTIVE_WINDOW_SHOW_TYPE_COUNT);
}

}  // namespace ash
