// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "ash/accelerators/accelerator_commands.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/accelerators/debug_commands.h"
#include "ash/ash_switches.h"
#include "ash/debug.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/focus_cycler.h"
#include "ash/gpu_support.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/ime_control_delegate.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/media_delegate.h"
#include "ash/multi_profile_uma.h"
#include "ash/new_window_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation.h"
#include "ash/screenshot_delegate.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_delegate.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/system/brightness_control_delegate.h"
#include "ash/system/keyboard_brightness/keyboard_brightness_control_delegate.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "ash/touch/touch_hud_debug.h"
#include "ash/volume_control_delegate.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/partial_screenshot_view.h"
#include "ash/wm/power_button_controller.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/user_metrics.h"
#include "ui/aura/env.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/compositor/debug_utils.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/debug_utils.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "ash/system/chromeos/keyboard_brightness_controller.h"
#include "base/sys_info.h"
#include "chromeos/ime/ime_keyboard.h"
#include "chromeos/ime/input_method_manager.h"
#endif  // defined(OS_CHROMEOS)

namespace ash {
namespace {

using base::UserMetricsAction;

bool DebugShortcutsEnabled() {
#if defined(NDEBUG)
  return CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshDebugShortcuts);
#else
  return true;
#endif
}

bool HandleAccessibleFocusCycle(bool reverse) {
  if (reverse) {
    base::RecordAction(UserMetricsAction("Accel_Accessible_Focus_Previous"));
  } else {
    base::RecordAction(UserMetricsAction("Accel_Accessible_Focus_Next"));
  }

  if (!Shell::GetInstance()->accessibility_delegate()->
      IsSpokenFeedbackEnabled()) {
    return false;
  }
  aura::Window* active_window = ash::wm::GetActiveWindow();
  if (!active_window)
    return false;
  views::Widget* widget =
      views::Widget::GetWidgetForNativeWindow(active_window);
  if (!widget)
    return false;
  views::FocusManager* focus_manager = widget->GetFocusManager();
  if (!focus_manager)
    return false;
  views::View* view = focus_manager->GetFocusedView();
  if (!view)
    return false;
  if (!strcmp(view->GetClassName(), views::WebView::kViewClassName))
    return false;

  focus_manager->AdvanceFocus(reverse);
  return true;
}

bool HandleCycleBackwardMRU(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_TAB)
    base::RecordAction(base::UserMetricsAction("Accel_PrevWindow_Tab"));

  Shell::GetInstance()->window_cycle_controller()->HandleCycleWindow(
      WindowCycleController::BACKWARD);
  return true;
}

bool HandleCycleForwardMRU(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_TAB)
    base::RecordAction(base::UserMetricsAction("Accel_NextWindow_Tab"));

  Shell::GetInstance()->window_cycle_controller()->HandleCycleWindow(
      WindowCycleController::FORWARD);
  return true;
}

bool ToggleOverview(const ui::Accelerator& accelerator) {
  base::RecordAction(base::UserMetricsAction("Accel_Overview_F5"));
  Shell::GetInstance()->window_selector_controller()->ToggleOverview();
  return true;
}

bool HandleFocusShelf() {
  Shell* shell = Shell::GetInstance();
  base::RecordAction(base::UserMetricsAction("Accel_Focus_Shelf"));
  return shell->focus_cycler()->FocusWidget(
      Shelf::ForPrimaryDisplay()->shelf_widget());
}

bool HandleLaunchAppN(int n) {
  base::RecordAction(UserMetricsAction("Accel_Launch_App"));
  Shelf::ForPrimaryDisplay()->LaunchAppIndexAt(n);
  return true;
}

bool HandleLaunchLastApp() {
  base::RecordAction(UserMetricsAction("Accel_Launch_Last_App"));
  Shelf::ForPrimaryDisplay()->LaunchAppIndexAt(-1);
  return true;
}

// Magnify the screen
bool HandleMagnifyScreen(int delta_index) {
  if (ash::Shell::GetInstance()->magnification_controller()->IsEnabled()) {
    // TODO(yoshiki): Move the following logic to MagnificationController.
    float scale =
        ash::Shell::GetInstance()->magnification_controller()->GetScale();
    // Calculate rounded logarithm (base kMagnificationScaleFactor) of scale.
    int scale_index =
        std::floor(std::log(scale) / std::log(kMagnificationScaleFactor) + 0.5);

    int new_scale_index = std::max(0, std::min(8, scale_index + delta_index));

    ash::Shell::GetInstance()->magnification_controller()->
        SetScale(std::pow(kMagnificationScaleFactor, new_scale_index), true);
  } else if (ash::Shell::GetInstance()->
             partial_magnification_controller()->is_enabled()) {
    float scale = delta_index > 0 ? kDefaultPartialMagnifiedScale : 1;
    ash::Shell::GetInstance()->partial_magnification_controller()->
        SetScale(scale);
  }

  return true;
}

bool HandleMediaNextTrack() {
  Shell::GetInstance()->media_delegate()->HandleMediaNextTrack();
  return true;
}

bool HandleMediaPlayPause() {
  Shell::GetInstance()->media_delegate()->HandleMediaPlayPause();
  return true;
}

bool HandleMediaPrevTrack() {
  Shell::GetInstance()->media_delegate()->HandleMediaPrevTrack();
  return true;
}

bool HandleNewIncognitoWindow() {
  base::RecordAction(UserMetricsAction("Accel_New_Incognito_Window"));
  bool incognito_allowed =
    Shell::GetInstance()->delegate()->IsIncognitoAllowed();
  if (incognito_allowed)
    Shell::GetInstance()->new_window_delegate()->NewWindow(
        true /* is_incognito */);
  return incognito_allowed;
}

bool HandleNewTab(ui::KeyboardCode key_code) {
  if (key_code == ui::VKEY_T)
    base::RecordAction(base::UserMetricsAction("Accel_NewTab_T"));
  Shell::GetInstance()->new_window_delegate()->NewTab();
  return true;
}

bool HandleNewWindow() {
  base::RecordAction(base::UserMetricsAction("Accel_New_Window"));
  Shell::GetInstance()->new_window_delegate()->NewWindow(
      false /* is_incognito */);
  return true;
}

void HandleNextIme(ImeControlDelegate* ime_control_delegate,
                   ui::EventType previous_event_type,
                   ui::KeyboardCode previous_key_code) {
  // This check is necessary e.g. not to process the Shift+Alt+
  // ET_KEY_RELEASED accelerator for Chrome OS (see ash/accelerators/
  // accelerator_controller.cc) when Shift+Alt+Tab is pressed and then Tab
  // is released.
  if (previous_event_type == ui::ET_KEY_RELEASED &&
      // Workaround for crbug.com/139556. CJK IME users tend to press
      // Enter (or Space) and Shift+Alt almost at the same time to commit
      // an IME string and then switch from the IME to the English layout.
      // This workaround allows the user to trigger NEXT_IME even if the
      // user presses Shift+Alt before releasing Enter.
      // TODO(nona|mazda): Fix crbug.com/139556 in a cleaner way.
      previous_key_code != ui::VKEY_RETURN &&
      previous_key_code != ui::VKEY_SPACE) {
    // We totally ignore this accelerator.
    // TODO(mazda): Fix crbug.com/158217
    return;
  }
  base::RecordAction(UserMetricsAction("Accel_Next_Ime"));
  if (ime_control_delegate)
    ime_control_delegate->HandleNextIme();
}

bool HandleOpenFeedbackPage() {
  base::RecordAction(UserMetricsAction("Accel_Open_Feedback_Page"));
  ash::Shell::GetInstance()->new_window_delegate()->OpenFeedbackPage();
  return true;
}

bool HandlePositionCenter() {
  base::RecordAction(UserMetricsAction("Accel_Window_Position_Center"));
  aura::Window* window = wm::GetActiveWindow();
  // Docked windows do not support centering and ignore accelerator.
  if (window && !wm::GetWindowState(window)->IsDocked()) {
    wm::CenterWindow(window);
    return true;
  }
  return false;
}

bool HandlePreviousIme(ImeControlDelegate* ime_control_delegate,
                       const ui::Accelerator& accelerator) {
  base::RecordAction(UserMetricsAction("Accel_Previous_Ime"));
  if (ime_control_delegate)
    return ime_control_delegate->HandlePreviousIme(accelerator);
  return false;
}

bool HandleRestoreTab() {
  base::RecordAction(base::UserMetricsAction("Accel_Restore_Tab"));
  Shell::GetInstance()->new_window_delegate()->RestoreTab();
  return true;
}

bool HandleRotatePaneFocus(Shell::Direction direction) {
  Shell* shell = Shell::GetInstance();
  switch (direction) {
    // TODO(stevet): Not sure if this is the same as IDC_FOCUS_NEXT_PANE.
    case Shell::FORWARD: {
      base::RecordAction(UserMetricsAction("Accel_Focus_Next_Pane"));
      shell->focus_cycler()->RotateFocus(FocusCycler::FORWARD);
      break;
    }
    case Shell::BACKWARD: {
      base::RecordAction(UserMetricsAction("Accel_Focus_Previous_Pane"));
      shell->focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
      break;
    }
  }
  return true;
}

// Rotate the active window.
bool HandleRotateActiveWindow() {
  base::RecordAction(UserMetricsAction("Accel_Rotate_Window"));
  aura::Window* active_window = wm::GetActiveWindow();
  if (active_window) {
    // The rotation animation bases its target transform on the current
    // rotation and position. Since there could be an animation in progress
    // right now, queue this animation so when it starts it picks up a neutral
    // rotation and position. Use replace so we only enqueue one at a time.
    active_window->layer()->GetAnimator()->
        set_preemption_strategy(ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
    active_window->layer()->GetAnimator()->StartAnimation(
        new ui::LayerAnimationSequence(
            new ash::ScreenRotation(360, active_window->layer())));
  }
  return true;
}

gfx::Display::Rotation GetNextRotation(gfx::Display::Rotation current) {
  switch (current) {
    case gfx::Display::ROTATE_0:
      return gfx::Display::ROTATE_90;
    case gfx::Display::ROTATE_90:
      return gfx::Display::ROTATE_180;
    case gfx::Display::ROTATE_180:
      return gfx::Display::ROTATE_270;
    case gfx::Display::ROTATE_270:
      return gfx::Display::ROTATE_0;
  }
  NOTREACHED() << "Unknown rotation:" << current;
  return gfx::Display::ROTATE_0;
}

// Rotates the screen.
bool HandleRotateScreen() {
  base::RecordAction(UserMetricsAction("Accel_Rotate_Window"));
  gfx::Point point = Shell::GetScreen()->GetCursorScreenPoint();
  gfx::Display display = Shell::GetScreen()->GetDisplayNearestPoint(point);
  const DisplayInfo& display_info =
      Shell::GetInstance()->display_manager()->GetDisplayInfo(display.id());
  Shell::GetInstance()->display_manager()->SetDisplayRotation(
      display.id(), GetNextRotation(display_info.rotation()));
  return true;
}

bool HandleScaleReset() {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  int64 display_id = display_manager->GetDisplayIdForUIScaling();
  if (display_id == gfx::Display::kInvalidDisplayID)
    return false;

  base::RecordAction(UserMetricsAction("Accel_Scale_Ui_Reset"));

  display_manager->SetDisplayUIScale(display_id, 1.0f);
  return true;
}

bool HandleScaleUI(bool up) {
  DisplayManager* display_manager = Shell::GetInstance()->display_manager();
  int64 display_id = display_manager->GetDisplayIdForUIScaling();
  if (display_id == gfx::Display::kInvalidDisplayID)
    return false;

  if (up) {
    base::RecordAction(UserMetricsAction("Accel_Scale_Ui_Up"));
  } else {
    base::RecordAction(UserMetricsAction("Accel_Scale_Ui_Down"));
  }

  const DisplayInfo& display_info = display_manager->GetDisplayInfo(display_id);
  float next_scale = DisplayManager::GetNextUIScale(display_info, up);
  display_manager->SetDisplayUIScale(display_id, next_scale);
  return true;
}

#if defined(OS_CHROMEOS)
bool HandleSwapPrimaryDisplay() {
  base::RecordAction(UserMetricsAction("Accel_Swap_Primary_Display"));
  Shell::GetInstance()->display_controller()->SwapPrimaryDisplay();
  return true;
}
#endif

bool HandleShowKeyboardOverlay() {
  base::RecordAction(UserMetricsAction("Accel_Show_Keyboard_Overlay"));
  ash::Shell::GetInstance()->new_window_delegate()->ShowKeyboardOverlay();

  return true;
}

void HandleShowMessageCenterBubble() {
  base::RecordAction(UserMetricsAction("Accel_Show_Message_Center_Bubble"));
  RootWindowController* controller =
      RootWindowController::ForTargetRootWindow();
  StatusAreaWidget* status_area_widget =
      controller->shelf()->status_area_widget();
  if (status_area_widget) {
    WebNotificationTray* notification_tray =
      status_area_widget->web_notification_tray();
    if (notification_tray->visible())
      notification_tray->ShowMessageCenterBubble();
  }
}

bool HandleShowSystemTrayBubble() {
  base::RecordAction(UserMetricsAction("Accel_Show_System_Tray_Bubble"));
  RootWindowController* controller =
      RootWindowController::ForTargetRootWindow();
  if (!controller->GetSystemTray()->HasSystemBubble()) {
    controller->GetSystemTray()->ShowDefaultView(BUBBLE_CREATE_NEW);
    return true;
  }
  return false;
}

bool HandleShowTaskManager() {
  base::RecordAction(UserMetricsAction("Accel_Show_Task_Manager"));
  Shell::GetInstance()->new_window_delegate()->ShowTaskManager();
  return true;
}

#if defined(OS_CHROMEOS)
void HandleSilenceSpokenFeedback() {
  base::RecordAction(UserMetricsAction("Accel_Silence_Spoken_Feedback"));

  AccessibilityDelegate* delegate =
      Shell::GetInstance()->accessibility_delegate();
  if (!delegate->IsSpokenFeedbackEnabled())
    return;
  delegate->SilenceSpokenFeedback();
}
#endif

bool HandleSwitchIme(ImeControlDelegate* ime_control_delegate,
                     const ui::Accelerator& accelerator) {
  base::RecordAction(UserMetricsAction("Accel_Switch_Ime"));
  if (ime_control_delegate)
    return ime_control_delegate->HandleSwitchIme(accelerator);
  return false;
}

bool HandleTakePartialScreenshot(ScreenshotDelegate* screenshot_delegate) {
  base::RecordAction(UserMetricsAction("Accel_Take_Partial_Screenshot"));
  if (screenshot_delegate) {
    ash::PartialScreenshotView::StartPartialScreenshot(
        screenshot_delegate);
  }
  // Return true to prevent propagation of the key event because
  // this key combination is reserved for partial screenshot.
  return true;
}

bool HandleTakeScreenshot(ScreenshotDelegate* screenshot_delegate) {
  base::RecordAction(UserMetricsAction("Accel_Take_Screenshot"));
  if (screenshot_delegate &&
      screenshot_delegate->CanTakeScreenshot()) {
    screenshot_delegate->HandleTakeScreenshotForAllRootWindows();
  }
  // Return true to prevent propagation of the key event.
  return true;
}

bool HandleToggleAppList(ui::KeyboardCode key_code,
                         ui::EventType previous_event_type,
                         ui::KeyboardCode previous_key_code,
                         const ui::Accelerator& accelerator) {
  // If something else was pressed between the Search key (LWIN)
  // being pressed and released, then ignore the release of the
  // Search key.
  if (key_code == ui::VKEY_LWIN &&
      (previous_event_type == ui::ET_KEY_RELEASED ||
       previous_key_code != ui::VKEY_LWIN))
    return false;
  if (key_code == ui::VKEY_LWIN)
    base::RecordAction(base::UserMetricsAction("Accel_Search_LWin"));
  // When spoken feedback is enabled, we should neither toggle the list nor
  // consume the key since Search+Shift is one of the shortcuts the a11y
  // feature uses. crbug.com/132296
  DCHECK_EQ(ui::VKEY_LWIN, accelerator.key_code());
  if (Shell::GetInstance()->accessibility_delegate()->
      IsSpokenFeedbackEnabled())
    return false;
  ash::Shell::GetInstance()->ToggleAppList(NULL);
  return true;
}

bool HandleToggleFullscreen(ui::KeyboardCode key_code) {
  if (key_code == ui::VKEY_MEDIA_LAUNCH_APP2) {
    base::RecordAction(UserMetricsAction("Accel_Fullscreen_F4"));
  }
  accelerators::ToggleFullscreen();
  return true;
}

bool HandleToggleRootWindowFullScreen() {
  Shell::GetPrimaryRootWindowController()->ash_host()->ToggleFullScreen();
  return true;
}

bool HandleWindowSnap(int action) {
  wm::WindowState* window_state = wm::GetActiveWindowState();
  // Disable window snapping shortcut key for full screen window due to
  // http://crbug.com/135487.
  if (!window_state ||
      window_state->window()->type() != ui::wm::WINDOW_TYPE_NORMAL ||
      window_state->IsFullscreen() ||
      !window_state->CanSnap()) {
    return false;
  }

  if (action == WINDOW_SNAP_LEFT) {
    base::RecordAction(UserMetricsAction("Accel_Window_Snap_Left"));
  } else {
    base::RecordAction(UserMetricsAction("Accel_Window_Snap_Right"));
  }
  const wm::WMEvent event(action == WINDOW_SNAP_LEFT ?
                          wm::WM_EVENT_SNAP_LEFT : wm::WM_EVENT_SNAP_RIGHT);
  window_state->OnWMEvent(&event);
  return true;
}

bool HandleWindowMinimize() {
  base::RecordAction(
      base::UserMetricsAction("Accel_Toggle_Minimized_Minus"));
  return accelerators::ToggleMinimized();
}

#if defined(OS_CHROMEOS)
bool HandleAddRemoveDisplay() {
  base::RecordAction(UserMetricsAction("Accel_Add_Remove_Display"));
  Shell::GetInstance()->display_manager()->AddRemoveDisplay();
  return true;
}

bool HandleCrosh() {
  base::RecordAction(UserMetricsAction("Accel_Open_Crosh"));

  Shell::GetInstance()->new_window_delegate()->OpenCrosh();
  return true;
}

bool HandleFileManager() {
  base::RecordAction(UserMetricsAction("Accel_Open_File_Manager"));

  Shell::GetInstance()->new_window_delegate()->OpenFileManager();
  return true;
}

bool HandleLock(ui::KeyboardCode key_code) {
  base::RecordAction(UserMetricsAction("Accel_LockScreen_L"));
  Shell::GetInstance()->session_state_delegate()->LockScreen();
  return true;
}

bool HandleCycleUser(SessionStateDelegate::CycleUser cycle_user) {
  if (!Shell::GetInstance()->delegate()->IsMultiProfilesEnabled())
    return false;
  ash::SessionStateDelegate* delegate =
      ash::Shell::GetInstance()->session_state_delegate();
  if (delegate->NumberOfLoggedInUsers() <= 1)
    return false;
  MultiProfileUMA::RecordSwitchActiveUser(
      MultiProfileUMA::SWITCH_ACTIVE_USER_BY_ACCELERATOR);
  switch (cycle_user) {
    case SessionStateDelegate::CYCLE_TO_NEXT_USER:
      base::RecordAction(UserMetricsAction("Accel_Switch_To_Next_User"));
      break;
    case SessionStateDelegate::CYCLE_TO_PREVIOUS_USER:
      base::RecordAction(UserMetricsAction("Accel_Switch_To_Previous_User"));
      break;
  }
  delegate->CycleActiveUser(cycle_user);
  return true;
}

bool HandleToggleMirrorMode() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Mirror_Mode"));
  Shell::GetInstance()->display_controller()->ToggleMirrorMode();
  return true;
}

bool HandleToggleSpokenFeedback() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Spoken_Feedback"));

  Shell::GetInstance()->accessibility_delegate()->
      ToggleSpokenFeedback(A11Y_NOTIFICATION_SHOW);
  return true;
}

bool HandleToggleTouchViewTesting() {
  // TODO(skuhne): This is only temporary! Remove this!
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableTouchViewTesting)) {
    MaximizeModeController* controller = Shell::GetInstance()->
        maximize_mode_controller();
    controller->EnableMaximizeModeWindowManager(
        !controller->IsMaximizeModeWindowManagerEnabled());
    return true;
  }
  return false;
}

bool HandleTouchHudClear() {
  RootWindowController* controller =
      RootWindowController::ForTargetRootWindow();
  if (controller->touch_hud_debug()) {
    controller->touch_hud_debug()->Clear();
    return true;
  }
  return false;
}

bool HandleTouchHudModeChange() {
  RootWindowController* controller =
      RootWindowController::ForTargetRootWindow();
  if (controller->touch_hud_debug()) {
    controller->touch_hud_debug()->ChangeToNextMode();
    return true;
  }
  return false;
}

bool HandleTouchHudProjectToggle() {
  base::RecordAction(UserMetricsAction("Accel_Touch_Hud_Clear"));
  bool enabled = Shell::GetInstance()->is_touch_hud_projection_enabled();
  Shell::GetInstance()->SetTouchHudProjectionEnabled(!enabled);
  return true;
}

bool HandleDisableCapsLock(ui::KeyboardCode key_code,
                           ui::EventType previous_event_type,
                           ui::KeyboardCode previous_key_code) {
  if (previous_event_type == ui::ET_KEY_RELEASED ||
      (previous_key_code != ui::VKEY_LSHIFT &&
       previous_key_code != ui::VKEY_SHIFT &&
       previous_key_code != ui::VKEY_RSHIFT)) {
    // If something else was pressed between the Shift key being pressed
    // and released, then ignore the release of the Shift key.
    return false;
  }
  base::RecordAction(UserMetricsAction("Accel_Disable_Caps_Lock"));
  chromeos::input_method::InputMethodManager* ime =
      chromeos::input_method::InputMethodManager::Get();
  chromeos::input_method::ImeKeyboard* keyboard =
      ime ? ime->GetImeKeyboard() : NULL;
  if (keyboard && keyboard->CapsLockIsEnabled()) {
    keyboard->SetCapsLockEnabled(false);
    return true;
  }
  return false;
}

bool HandleToggleCapsLock(ui::KeyboardCode key_code,
                          ui::EventType previous_event_type,
                          ui::KeyboardCode previous_key_code) {
  if (key_code == ui::VKEY_LWIN) {
    // If something else was pressed between the Search key (LWIN)
    // being pressed and released, then ignore the release of the
    // Search key.
    // TODO(danakj): Releasing Alt first breaks this: crbug.com/166495
    if (previous_event_type == ui::ET_KEY_RELEASED ||
        previous_key_code != ui::VKEY_LWIN)
      return false;
  }
  base::RecordAction(UserMetricsAction("Accel_Toggle_Caps_Lock"));
  chromeos::input_method::InputMethodManager* ime =
      chromeos::input_method::InputMethodManager::Get();
  chromeos::input_method::ImeKeyboard* keyboard =
      ime ? ime->GetImeKeyboard() : NULL;
  if (keyboard)
    keyboard->SetCapsLockEnabled(!keyboard->CapsLockIsEnabled());
  return true;
}

#endif  // defined(OS_CHROMEOS)

// Debug print methods.

bool HandlePrintLayerHierarchy() {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (size_t i = 0; i < root_windows.size(); ++i) {
    ui::PrintLayerHierarchy(
        root_windows[i]->layer(),
        root_windows[i]->GetHost()->dispatcher()->GetLastMouseLocationInRoot());
  }
  return true;
}

bool HandlePrintViewHierarchy() {
  aura::Window* active_window = ash::wm::GetActiveWindow();
  if (!active_window)
    return true;
  views::Widget* browser_widget =
      views::Widget::GetWidgetForNativeWindow(active_window);
  if (!browser_widget)
    return true;
  views::PrintViewHierarchy(browser_widget->GetRootView());
  return true;
}

void PrintWindowHierarchy(aura::Window* window,
                          int indent,
                          std::ostringstream* out) {
  std::string indent_str(indent, ' ');
  std::string name(window->name());
  if (name.empty())
    name = "\"\"";
  *out << indent_str << name << " (" << window << ")"
       << " type=" << window->type()
       << (wm::IsActiveWindow(window) ? " [active] " : " ")
       << (window->IsVisible() ? " visible " : " ")
       << window->bounds().ToString()
       << '\n';

  for (size_t i = 0; i < window->children().size(); ++i)
    PrintWindowHierarchy(window->children()[i], indent + 3, out);
}

bool HandlePrintWindowHierarchy() {
  Shell::RootWindowControllerList controllers =
      Shell::GetAllRootWindowControllers();
  for (size_t i = 0; i < controllers.size(); ++i) {
    std::ostringstream out;
    out << "RootWindow " << i << ":\n";
    PrintWindowHierarchy(controllers[i]->GetRootWindow(), 0, &out);
    // Error so logs can be collected from end-users.
    LOG(ERROR) << out.str();
  }
  return true;
}

bool HandlePrintUIHierarchies() {
  // This is a separate command so the user only has to hit one key to generate
  // all the logs. Developers use the individual dumps repeatedly, so keep
  // those as separate commands to avoid spamming their logs.
  HandlePrintLayerHierarchy();
  HandlePrintWindowHierarchy();
  HandlePrintViewHierarchy();
  return true;
}

class AutoSet {
 public:
  AutoSet(ui::Accelerator* scoped, ui::Accelerator new_value)
      : scoped_(scoped), new_value_(new_value) {}
  ~AutoSet() { *scoped_ = new_value_; }

 private:
  ui::Accelerator* scoped_;
  const ui::Accelerator new_value_;

  DISALLOW_COPY_AND_ASSIGN(AutoSet);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AcceleratorController, public:

AcceleratorController::AcceleratorController()
    : accelerator_manager_(new ui::AcceleratorManager) {
  Init();
}

AcceleratorController::~AcceleratorController() {
}

void AcceleratorController::Init() {
  previous_accelerator_.set_type(ui::ET_UNKNOWN);
  for (size_t i = 0; i < kActionsAllowedAtLoginOrLockScreenLength; ++i) {
    actions_allowed_at_login_screen_.insert(
        kActionsAllowedAtLoginOrLockScreen[i]);
    actions_allowed_at_lock_screen_.insert(
        kActionsAllowedAtLoginOrLockScreen[i]);
  }
  for (size_t i = 0; i < kActionsAllowedAtLockScreenLength; ++i)
    actions_allowed_at_lock_screen_.insert(kActionsAllowedAtLockScreen[i]);
  for (size_t i = 0; i < kActionsAllowedAtModalWindowLength; ++i)
    actions_allowed_at_modal_window_.insert(kActionsAllowedAtModalWindow[i]);
  for (size_t i = 0; i < kPreferredActionsLength; ++i)
    preferred_actions_.insert(kPreferredActions[i]);
  for (size_t i = 0; i < kReservedActionsLength; ++i)
    reserved_actions_.insert(kReservedActions[i]);
  for (size_t i = 0; i < kNonrepeatableActionsLength; ++i)
    nonrepeatable_actions_.insert(kNonrepeatableActions[i]);
  for (size_t i = 0; i < kActionsAllowedInAppModeLength; ++i)
    actions_allowed_in_app_mode_.insert(kActionsAllowedInAppMode[i]);
  for (size_t i = 0; i < kActionsNeedingWindowLength; ++i)
    actions_needing_window_.insert(kActionsNeedingWindow[i]);

  RegisterAccelerators(kAcceleratorData, kAcceleratorDataLength);

#if !defined(NDEBUG)
  RegisterAccelerators(kDesktopAcceleratorData, kDesktopAcceleratorDataLength);
#endif

  if (DebugShortcutsEnabled()) {
    RegisterAccelerators(kDebugAcceleratorData, kDebugAcceleratorDataLength);
    for (size_t i = 0; i < kReservedDebugActionsLength; ++i)
      reserved_actions_.insert(kReservedDebugActions[i]);
  }

#if defined(OS_CHROMEOS)
  keyboard_brightness_control_delegate_.reset(
      new KeyboardBrightnessController());
#endif
}

void AcceleratorController::Register(const ui::Accelerator& accelerator,
                                     ui::AcceleratorTarget* target) {
  accelerator_manager_->Register(accelerator,
                                 ui::AcceleratorManager::kNormalPriority,
                                 target);
}

void AcceleratorController::Unregister(const ui::Accelerator& accelerator,
                                       ui::AcceleratorTarget* target) {
  accelerator_manager_->Unregister(accelerator, target);
}

void AcceleratorController::UnregisterAll(ui::AcceleratorTarget* target) {
  accelerator_manager_->UnregisterAll(target);
}

bool AcceleratorController::Process(const ui::Accelerator& accelerator) {
  AutoSet auto_set(&previous_accelerator_, accelerator);

  if (ime_control_delegate_) {
    return accelerator_manager_->Process(
        ime_control_delegate_->RemapAccelerator(accelerator));
  }
  return accelerator_manager_->Process(accelerator);
}

bool AcceleratorController::IsRegistered(
    const ui::Accelerator& accelerator) const {
  return accelerator_manager_->GetCurrentTarget(accelerator) != NULL;
}

bool AcceleratorController::IsPreferred(
    const ui::Accelerator& accelerator) const {
  const ui::Accelerator remapped_accelerator = ime_control_delegate_.get() ?
      ime_control_delegate_->RemapAccelerator(accelerator) : accelerator;

  std::map<ui::Accelerator, int>::const_iterator iter =
      accelerators_.find(remapped_accelerator);
  if (iter == accelerators_.end())
    return false;  // not an accelerator.

  return preferred_actions_.find(iter->second) != preferred_actions_.end();
}

bool AcceleratorController::IsReserved(
    const ui::Accelerator& accelerator) const {
  const ui::Accelerator remapped_accelerator = ime_control_delegate_.get() ?
      ime_control_delegate_->RemapAccelerator(accelerator) : accelerator;

  std::map<ui::Accelerator, int>::const_iterator iter =
      accelerators_.find(remapped_accelerator);
  if (iter == accelerators_.end())
    return false;  // not an accelerator.

  return reserved_actions_.find(iter->second) != reserved_actions_.end();
}

bool AcceleratorController::PerformAction(int action,
                                          const ui::Accelerator& accelerator) {
  ash::Shell* shell = ash::Shell::GetInstance();
  AcceleratorProcessingRestriction restriction =
      GetAcceleratorProcessingRestriction(action);
  if (restriction != RESTRICTION_NONE)
    return restriction == RESTRICTION_PREVENT_PROCESSING_AND_PROPAGATION;

  const ui::KeyboardCode key_code = accelerator.key_code();
  // PerformAction() is performed from gesture controllers and passes
  // empty Accelerator() instance as the second argument. Such events
  // should never be suspended.
  const bool gesture_event = key_code == ui::VKEY_UNKNOWN;
  // Ignore accelerators invoked as repeated (while holding a key for a long
  // time, if their handling is nonrepeatable.
  if (nonrepeatable_actions_.find(action) != nonrepeatable_actions_.end() &&
      accelerator.IsRepeat() && !gesture_event) {
    return true;
  }
  // Type of the previous accelerator. Used by NEXT_IME and DISABLE_CAPS_LOCK.
  const ui::EventType previous_event_type = previous_accelerator_.type();
  const ui::KeyboardCode previous_key_code = previous_accelerator_.key_code();

  // You *MUST* return true when some action is performed. Otherwise, this
  // function might be called *twice*, via BrowserView::PreHandleKeyboardEvent
  // and BrowserView::HandleKeyboardEvent, for a single accelerator press.
  //
  // If your accelerator invokes more than one line of code, please either
  // implement it in your module's controller code (like TOGGLE_MIRROR_MODE
  // below) or pull it into a HandleFoo() function above.
  switch (action) {
    case ACCESSIBLE_FOCUS_NEXT:
      return HandleAccessibleFocusCycle(false);
    case ACCESSIBLE_FOCUS_PREVIOUS:
      return HandleAccessibleFocusCycle(true);
    case CYCLE_BACKWARD_MRU:
      return HandleCycleBackwardMRU(accelerator);
    case CYCLE_FORWARD_MRU:
      return HandleCycleForwardMRU(accelerator);
    case TOGGLE_OVERVIEW:
      return ToggleOverview(accelerator);
#if defined(OS_CHROMEOS)
    case ADD_REMOVE_DISPLAY:
      return HandleAddRemoveDisplay();
    case TOGGLE_MIRROR_MODE:
      return HandleToggleMirrorMode();
    case LOCK_SCREEN:
      return HandleLock(key_code);
    case OPEN_FILE_MANAGER:
      return HandleFileManager();
    case OPEN_CROSH:
      return HandleCrosh();
    case SILENCE_SPOKEN_FEEDBACK:
      HandleSilenceSpokenFeedback();
      break;
    case SWAP_PRIMARY_DISPLAY:
      return HandleSwapPrimaryDisplay();
    case SWITCH_TO_NEXT_USER:
      return HandleCycleUser(SessionStateDelegate::CYCLE_TO_NEXT_USER);
    case SWITCH_TO_PREVIOUS_USER:
      return HandleCycleUser(SessionStateDelegate::CYCLE_TO_PREVIOUS_USER);
    case TOGGLE_SPOKEN_FEEDBACK:
      return HandleToggleSpokenFeedback();
    case TOGGLE_TOUCH_VIEW_TESTING:
      return HandleToggleTouchViewTesting();
    case TOGGLE_WIFI:
      Shell::GetInstance()->system_tray_notifier()->NotifyRequestToggleWifi();
      return true;
    case TOUCH_HUD_CLEAR:
      return HandleTouchHudClear();
    case TOUCH_HUD_MODE_CHANGE:
      return HandleTouchHudModeChange();
    case TOUCH_HUD_PROJECTION_TOGGLE:
      return HandleTouchHudProjectToggle();
    case DISABLE_GPU_WATCHDOG:
      Shell::GetInstance()->gpu_support()->DisableGpuWatchdog();
      return true;
    case DISABLE_CAPS_LOCK:
      return HandleDisableCapsLock(
          key_code, previous_event_type, previous_key_code);
    case TOGGLE_CAPS_LOCK:
      return HandleToggleCapsLock(
          key_code, previous_event_type, previous_key_code);
#endif  // OS_CHROMEOS
    case OPEN_FEEDBACK_PAGE:
      return HandleOpenFeedbackPage();
    case EXIT:
      // UMA metrics are recorded in the handler.
      exit_warning_handler_.HandleAccelerator();
      return true;
    case NEW_INCOGNITO_WINDOW:
      return HandleNewIncognitoWindow();
    case NEW_TAB:
      return HandleNewTab(key_code);
    case NEW_WINDOW:
      return HandleNewWindow();
    case RESTORE_TAB:
      return HandleRestoreTab();
    case TAKE_SCREENSHOT:
      return HandleTakeScreenshot(screenshot_delegate_.get());
    case TAKE_PARTIAL_SCREENSHOT:
      return HandleTakePartialScreenshot(screenshot_delegate_.get());
    case TOGGLE_APP_LIST:
      return HandleToggleAppList(
          key_code, previous_event_type, previous_key_code, accelerator);
    case BRIGHTNESS_DOWN:
      if (brightness_control_delegate_)
        return brightness_control_delegate_->HandleBrightnessDown(accelerator);
      break;
    case BRIGHTNESS_UP:
      if (brightness_control_delegate_)
        return brightness_control_delegate_->HandleBrightnessUp(accelerator);
      break;
    case KEYBOARD_BRIGHTNESS_DOWN:
      if (keyboard_brightness_control_delegate_)
        return keyboard_brightness_control_delegate_->
            HandleKeyboardBrightnessDown(accelerator);
      break;
    case KEYBOARD_BRIGHTNESS_UP:
      if (keyboard_brightness_control_delegate_)
        return keyboard_brightness_control_delegate_->
            HandleKeyboardBrightnessUp(accelerator);
      break;
    case VOLUME_MUTE: {
      ash::VolumeControlDelegate* volume_delegate =
          shell->system_tray_delegate()->GetVolumeControlDelegate();
      return volume_delegate && volume_delegate->HandleVolumeMute(accelerator);
    }
    case VOLUME_DOWN: {
      ash::VolumeControlDelegate* volume_delegate =
          shell->system_tray_delegate()->GetVolumeControlDelegate();
      return volume_delegate && volume_delegate->HandleVolumeDown(accelerator);
    }
    case VOLUME_UP: {
      ash::VolumeControlDelegate* volume_delegate =
          shell->system_tray_delegate()->GetVolumeControlDelegate();
      return volume_delegate && volume_delegate->HandleVolumeUp(accelerator);
    }
    case FOCUS_SHELF:
      return HandleFocusShelf();
    case FOCUS_NEXT_PANE:
      return HandleRotatePaneFocus(Shell::FORWARD);
    case FOCUS_PREVIOUS_PANE:
      return HandleRotatePaneFocus(Shell::BACKWARD);
    case SHOW_KEYBOARD_OVERLAY:
      return HandleShowKeyboardOverlay();
    case SHOW_SYSTEM_TRAY_BUBBLE:
      return HandleShowSystemTrayBubble();
    case SHOW_MESSAGE_CENTER_BUBBLE:
      HandleShowMessageCenterBubble();
      break;
    case SHOW_TASK_MANAGER:
      return HandleShowTaskManager();
    case NEXT_IME:
      HandleNextIme(
          ime_control_delegate_.get(), previous_event_type, previous_key_code);
      // NEXT_IME is bound to Alt-Shift key up event. To be consistent with
      // Windows behavior, do not consume the key event here.
      return false;
    case PREVIOUS_IME:
      return HandlePreviousIme(ime_control_delegate_.get(), accelerator);
    case PRINT_UI_HIERARCHIES:
      return HandlePrintUIHierarchies();
    case SWITCH_IME:
      return HandleSwitchIme(ime_control_delegate_.get(), accelerator);
    case LAUNCH_APP_0:
      return HandleLaunchAppN(0);
    case LAUNCH_APP_1:
      return HandleLaunchAppN(1);
    case LAUNCH_APP_2:
      return HandleLaunchAppN(2);
    case LAUNCH_APP_3:
      return HandleLaunchAppN(3);
    case LAUNCH_APP_4:
      return HandleLaunchAppN(4);
    case LAUNCH_APP_5:
      return HandleLaunchAppN(5);
    case LAUNCH_APP_6:
      return HandleLaunchAppN(6);
    case LAUNCH_APP_7:
      return HandleLaunchAppN(7);
    case LAUNCH_LAST_APP:
      return HandleLaunchLastApp();
    case WINDOW_SNAP_LEFT:
    case WINDOW_SNAP_RIGHT:
      return HandleWindowSnap(action);
    case WINDOW_MINIMIZE:
      return HandleWindowMinimize();
    case TOGGLE_FULLSCREEN:
      return HandleToggleFullscreen(key_code);
    case TOGGLE_MAXIMIZED:
      accelerators::ToggleMaximized();
      return true;
    case WINDOW_POSITION_CENTER:
     return HandlePositionCenter();
    case SCALE_UI_UP:
      return HandleScaleUI(true /* up */);
    case SCALE_UI_DOWN:
      return HandleScaleUI(false /* down */);
    case SCALE_UI_RESET:
      return HandleScaleReset();
    case ROTATE_WINDOW:
      return HandleRotateActiveWindow();
    case ROTATE_SCREEN:
      return HandleRotateScreen();
    case TOGGLE_DESKTOP_BACKGROUND_MODE:
      return debug::CycleDesktopBackgroundMode();
    case TOGGLE_ROOT_WINDOW_FULL_SCREEN:
      return HandleToggleRootWindowFullScreen();
    case DEBUG_TOGGLE_DEVICE_SCALE_FACTOR:
      Shell::GetInstance()->display_manager()->ToggleDisplayScaleFactor();
      return true;
    case DEBUG_TOGGLE_SHOW_DEBUG_BORDERS:
      ash::debug::ToggleShowDebugBorders();
      return true;
    case DEBUG_TOGGLE_SHOW_FPS_COUNTER:
      ash::debug::ToggleShowFpsCounter();
      return true;
    case DEBUG_TOGGLE_SHOW_PAINT_RECTS:
      ash::debug::ToggleShowPaintRects();
      return true;
    case MAGNIFY_SCREEN_ZOOM_IN:
      return HandleMagnifyScreen(1);
    case MAGNIFY_SCREEN_ZOOM_OUT:
      return HandleMagnifyScreen(-1);
    case MEDIA_NEXT_TRACK:
      return HandleMediaNextTrack();
    case MEDIA_PLAY_PAUSE:
       return HandleMediaPlayPause();
    case MEDIA_PREV_TRACK:
       return HandleMediaPrevTrack();
    case POWER_PRESSED:  // fallthrough
    case POWER_RELEASED:
#if defined(OS_CHROMEOS)
      if (!base::SysInfo::IsRunningOnChromeOS()) {
        // There is no powerd in linux desktop, so call the
        // PowerButtonController here.
        Shell::GetInstance()->power_button_controller()->
            OnPowerButtonEvent(action == POWER_PRESSED, base::TimeTicks());
      }
#endif
      // We don't do anything with these at present on the device,
      // (power button events are reported to us from powerm via
      // D-BUS), but we consume them to prevent them from getting
      // passed to apps -- see http://crbug.com/146609.
      return true;
    case LOCK_PRESSED:
    case LOCK_RELEASED:
      Shell::GetInstance()->power_button_controller()->
          OnLockButtonEvent(action == LOCK_PRESSED, base::TimeTicks());
      return true;
    case PRINT_LAYER_HIERARCHY:
      return HandlePrintLayerHierarchy();
    case PRINT_VIEW_HIERARCHY:
      return HandlePrintViewHierarchy();
    case PRINT_WINDOW_HIERARCHY:
      return HandlePrintWindowHierarchy();
    default:
      NOTREACHED() << "Unhandled action " << action;
  }
  return false;
}

AcceleratorController::AcceleratorProcessingRestriction
AcceleratorController::GetCurrentAcceleratorRestriction() {
  return GetAcceleratorProcessingRestriction(-1);
}

AcceleratorController::AcceleratorProcessingRestriction
AcceleratorController::GetAcceleratorProcessingRestriction(int action) {
  ash::Shell* shell = ash::Shell::GetInstance();
  if (!shell->session_state_delegate()->IsActiveUserSessionStarted() &&
      actions_allowed_at_login_screen_.find(action) ==
          actions_allowed_at_login_screen_.end()) {
    return RESTRICTION_PREVENT_PROCESSING;
  }
  if (shell->session_state_delegate()->IsScreenLocked() &&
      actions_allowed_at_lock_screen_.find(action) ==
          actions_allowed_at_lock_screen_.end()) {
    return RESTRICTION_PREVENT_PROCESSING;
  }
  if (shell->IsSystemModalWindowOpen() &&
      actions_allowed_at_modal_window_.find(action) ==
          actions_allowed_at_modal_window_.end()) {
    // Note we prevent the shortcut from propagating so it will not
    // be passed to the modal window. This is important for things like
    // Alt+Tab that would cause an undesired effect in the modal window by
    // cycling through its window elements.
    return RESTRICTION_PREVENT_PROCESSING_AND_PROPAGATION;
  }
  if (shell->delegate()->IsRunningInForcedAppMode() &&
      actions_allowed_in_app_mode_.find(action) ==
          actions_allowed_in_app_mode_.end()) {
    return RESTRICTION_PREVENT_PROCESSING;
  }
  if (MruWindowTracker::BuildWindowList(false).empty() &&
      actions_needing_window_.find(action) != actions_needing_window_.end()) {
    Shell::GetInstance()->accessibility_delegate()->TriggerAccessibilityAlert(
        A11Y_ALERT_WINDOW_NEEDED);
    return RESTRICTION_PREVENT_PROCESSING_AND_PROPAGATION;
  }
  return RESTRICTION_NONE;
}

void AcceleratorController::SetBrightnessControlDelegate(
    scoped_ptr<BrightnessControlDelegate> brightness_control_delegate) {
  brightness_control_delegate_ = brightness_control_delegate.Pass();
}

void AcceleratorController::SetImeControlDelegate(
    scoped_ptr<ImeControlDelegate> ime_control_delegate) {
  ime_control_delegate_ = ime_control_delegate.Pass();
}

void AcceleratorController::SetScreenshotDelegate(
    scoped_ptr<ScreenshotDelegate> screenshot_delegate) {
  screenshot_delegate_ = screenshot_delegate.Pass();
}

////////////////////////////////////////////////////////////////////////////////
// AcceleratorController, ui::AcceleratorTarget implementation:

bool AcceleratorController::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  std::map<ui::Accelerator, int>::const_iterator it =
      accelerators_.find(accelerator);
  DCHECK(it != accelerators_.end());
  return PerformAction(static_cast<AcceleratorAction>(it->second), accelerator);
}

void AcceleratorController::RegisterAccelerators(
    const AcceleratorData accelerators[],
    size_t accelerators_length) {
  for (size_t i = 0; i < accelerators_length; ++i) {
    ui::Accelerator accelerator(accelerators[i].keycode,
                                accelerators[i].modifiers);
    accelerator.set_type(accelerators[i].trigger_on_press ?
                         ui::ET_KEY_PRESSED : ui::ET_KEY_RELEASED);
    Register(accelerator, this);
    accelerators_.insert(
        std::make_pair(accelerator, accelerators[i].action));
  }
}

void AcceleratorController::SetKeyboardBrightnessControlDelegate(
    scoped_ptr<KeyboardBrightnessControlDelegate>
    keyboard_brightness_control_delegate) {
  keyboard_brightness_control_delegate_ =
      keyboard_brightness_control_delegate.Pass();
}

bool AcceleratorController::CanHandleAccelerators() const {
  return true;
}

}  // namespace ash
