// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "ash/accelerators/accelerator_commands.h"
#include "ash/accelerators/debug_commands.h"
#include "ash/ash_switches.h"
#include "ash/debug.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/focus_cycler.h"
#include "ash/gpu_support.h"
#include "ash/ime_control_delegate.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/media_delegate.h"
#include "ash/multi_profile_uma.h"
#include "ash/new_window_delegate.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/rotator/window_rotation.h"
#include "ash/screen_util.h"
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
#include "ash/system/system_notifier.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "ash/touch/touch_hud_debug.h"
#include "ash/utility/partial_screenshot_controller.h"
#include "ash/volume_control_delegate.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/power_button_controller.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/env.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/screen.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notifier_settings.h"

#if defined(OS_CHROMEOS)
#include "ash/display/display_configuration_controller.h"
#include "ash/system/chromeos/keyboard_brightness_controller.h"
#include "base/sys_info.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#endif  // defined(OS_CHROMEOS)

namespace ash {
namespace {

using base::UserMetricsAction;

// The notification delegate that will be used to open the keyboard shortcut
// help page when the notification is clicked.
class DeprecatedAcceleratorNotificationDelegate
    : public message_center::NotificationDelegate {
 public:
  DeprecatedAcceleratorNotificationDelegate() {}

  // message_center::NotificationDelegate:
  bool HasClickedListener() override { return true; }

  void Click() override {
    if (!Shell::GetInstance()->session_state_delegate()->IsUserSessionBlocked())
      Shell::GetInstance()->delegate()->OpenKeyboardShortcutHelpPage();
  }

 private:
  // Private destructor since NotificationDelegate is ref-counted.
  ~DeprecatedAcceleratorNotificationDelegate() override {}

  DISALLOW_COPY_AND_ASSIGN(DeprecatedAcceleratorNotificationDelegate);
};

ui::Accelerator CreateAccelerator(ui::KeyboardCode keycode,
                                  int modifiers,
                                  bool trigger_on_press) {
  ui::Accelerator accelerator(keycode, modifiers);
  accelerator.set_type(trigger_on_press ? ui::ET_KEY_PRESSED
                                        : ui::ET_KEY_RELEASED);
  return accelerator;
}

// Ensures that there are no word breaks at the "+"s in the shortcut texts such
// as "Ctrl+Shift+Space".
void EnsureNoWordBreaks(base::string16* shortcut_text) {
  std::vector<base::string16> keys =
      base::SplitString(*shortcut_text, base::ASCIIToUTF16("+"),
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (keys.size() < 2U)
    return;

  // The plus sign surrounded by the word joiner to guarantee an non-breaking
  // shortcut.
  const base::string16 non_breaking_plus =
      base::UTF8ToUTF16("\xe2\x81\xa0+\xe2\x81\xa0");
  shortcut_text->clear();
  for (size_t i = 0; i < keys.size() - 1; ++i) {
    *shortcut_text += keys[i];
    *shortcut_text += non_breaking_plus;
  }

  *shortcut_text += keys[keys.size() - 1];
}

// Gets the notification message after it formats it in such a way that there
// are no line breaks in the middle of the shortcut texts.
base::string16 GetNotificationText(int message_id,
                                   int old_shortcut_id,
                                   int new_shortcut_id) {
  base::string16 old_shortcut = l10n_util::GetStringUTF16(old_shortcut_id);
  base::string16 new_shortcut = l10n_util::GetStringUTF16(new_shortcut_id);
  EnsureNoWordBreaks(&old_shortcut);
  EnsureNoWordBreaks(&new_shortcut);

  return l10n_util::GetStringFUTF16(message_id, new_shortcut, old_shortcut);
}

void ShowDeprecatedAcceleratorNotification(const char* const notification_id,
                                           int message_id,
                                           int old_shortcut_id,
                                           int new_shortcut_id) {
  const base::string16 message =
      GetNotificationText(message_id, old_shortcut_id, new_shortcut_id);
  scoped_ptr<message_center::Notification> notification(
      new message_center::Notification(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          base::string16(), message,
          Shell::GetInstance()->delegate()->GetDeprecatedAcceleratorImage(),
          base::string16(), GURL(),
          message_center::NotifierId(
              message_center::NotifierId::SYSTEM_COMPONENT,
              system_notifier::kNotifierDeprecatedAccelerator),
          message_center::RichNotificationData(),
          new DeprecatedAcceleratorNotificationDelegate));
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void RecordUmaHistogram(const char* histogram_name,
                        DeprecatedAcceleratorUsage sample) {
  auto histogram = base::LinearHistogram::FactoryGet(
      histogram_name, 1, DEPRECATED_USAGE_COUNT, DEPRECATED_USAGE_COUNT + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

void HandleCycleBackwardMRU(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_TAB)
    base::RecordAction(base::UserMetricsAction("Accel_PrevWindow_Tab"));

  Shell::GetInstance()->window_cycle_controller()->HandleCycleWindow(
      WindowCycleController::BACKWARD);
}

void HandleCycleForwardMRU(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_TAB)
    base::RecordAction(base::UserMetricsAction("Accel_NextWindow_Tab"));

  Shell::GetInstance()->window_cycle_controller()->HandleCycleWindow(
      WindowCycleController::FORWARD);
}

void HandleRotatePaneFocus(Shell::Direction direction) {
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
}

void HandleFocusShelf() {
  Shell* shell = Shell::GetInstance();
  base::RecordAction(base::UserMetricsAction("Accel_Focus_Shelf"));
  shell->focus_cycler()->FocusWidget(
      Shelf::ForPrimaryDisplay()->shelf_widget());
}

void HandleLaunchAppN(int n) {
  base::RecordAction(UserMetricsAction("Accel_Launch_App"));
  Shelf::ForPrimaryDisplay()->LaunchAppIndexAt(n);
}

void HandleLaunchLastApp() {
  base::RecordAction(UserMetricsAction("Accel_Launch_Last_App"));
  Shelf::ForPrimaryDisplay()->LaunchAppIndexAt(-1);
}

bool CanHandleMagnifyScreen() {
  Shell* shell = Shell::GetInstance();
  return shell->magnification_controller()->IsEnabled() ||
      shell->partial_magnification_controller()->is_enabled();
}

// Magnify the screen
void HandleMagnifyScreen(int delta_index) {
  if (ash::Shell::GetInstance()->magnification_controller()->IsEnabled()) {
    // TODO(yoshiki): Move the following logic to MagnificationController.
    float scale =
        ash::Shell::GetInstance()->magnification_controller()->GetScale();
    // Calculate rounded logarithm (base kMagnificationScaleFactor) of scale.
    int scale_index = std::floor(
        std::log(scale) / std::log(ui::kMagnificationScaleFactor) + 0.5);

    int new_scale_index = std::max(0, std::min(8, scale_index + delta_index));

    ash::Shell::GetInstance()->magnification_controller()->SetScale(
        std::pow(ui::kMagnificationScaleFactor, new_scale_index), true);
  } else if (ash::Shell::GetInstance()->
             partial_magnification_controller()->is_enabled()) {
    float scale = delta_index > 0 ? kDefaultPartialMagnifiedScale : 1;
    ash::Shell::GetInstance()->partial_magnification_controller()->
        SetScale(scale);
  }
}

void HandleMediaNextTrack() {
  Shell::GetInstance()->media_delegate()->HandleMediaNextTrack();
}

void HandleMediaPlayPause() {
  Shell::GetInstance()->media_delegate()->HandleMediaPlayPause();
}

void HandleMediaPrevTrack() {
  Shell::GetInstance()->media_delegate()->HandleMediaPrevTrack();
}

bool CanHandleNewIncognitoWindow() {
  return Shell::GetInstance()->delegate()->IsIncognitoAllowed();
}

void HandleNewIncognitoWindow() {
  base::RecordAction(UserMetricsAction("Accel_New_Incognito_Window"));
  Shell::GetInstance()->new_window_delegate()->NewWindow(
      true /* is_incognito */);
}

void HandleNewTab(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_T)
    base::RecordAction(base::UserMetricsAction("Accel_NewTab_T"));
  Shell::GetInstance()->new_window_delegate()->NewTab();
}

void HandleNewWindow() {
  base::RecordAction(base::UserMetricsAction("Accel_New_Window"));
  Shell::GetInstance()->new_window_delegate()->NewWindow(
      false /* is_incognito */);
}

bool CanHandleNextIme(ImeControlDelegate* ime_control_delegate,
                      const ui::Accelerator& previous_accelerator,
                      bool current_accelerator_is_deprecated) {
  if (current_accelerator_is_deprecated) {
    // We only allow next IME to be triggered if the previous is accelerator key
    // is ONLY either Shift, Alt, Enter or Space.
    // The first six cases below are to avoid conflicting accelerators that
    // contain Alt+Shift (like Alt+Shift+Tab or Alt+Shift+S) to trigger next IME
    // when the wrong order of key sequences is pressed. crbug.com/527154.
    // The latter two cases are needed for CJK IME users who tend to press Enter
    // (or Space) and Shift+Alt almost at the same time to commit an IME string
    // and then switch from the IME to the English layout. This allows these
    // users to trigger NEXT_IME even if they press Shift+Alt before releasing
    // Enter. crbug.com/139556.
    // TODO(nona|mazda): Fix crbug.com/139556 in a cleaner way.
    const ui::KeyboardCode previous_key_code = previous_accelerator.key_code();
    switch (previous_key_code) {
      case ui::VKEY_SHIFT:
      case ui::VKEY_LSHIFT:
      case ui::VKEY_RSHIFT:
      case ui::VKEY_MENU:
      case ui::VKEY_LMENU:
      case ui::VKEY_RMENU:
      case ui::VKEY_RETURN:
      case ui::VKEY_SPACE:
        break;

      default:
        return false;
    }
  }

  return ime_control_delegate && ime_control_delegate->CanCycleIme();
}

void HandleNextIme(ImeControlDelegate* ime_control_delegate) {
  base::RecordAction(UserMetricsAction("Accel_Next_Ime"));
  ime_control_delegate->HandleNextIme();
}

void HandleOpenFeedbackPage() {
  base::RecordAction(UserMetricsAction("Accel_Open_Feedback_Page"));
  ash::Shell::GetInstance()->new_window_delegate()->OpenFeedbackPage();
}

bool CanHandlePreviousIme(ImeControlDelegate* ime_control_delegate) {
  return ime_control_delegate && ime_control_delegate->CanCycleIme();
}

void HandlePreviousIme(ImeControlDelegate* ime_control_delegate,
                       const ui::Accelerator& accelerator) {
  base::RecordAction(UserMetricsAction("Accel_Previous_Ime"));
  if (accelerator.type() == ui::ET_KEY_PRESSED)
    ime_control_delegate->HandlePreviousIme();
  // Else: consume the Ctrl+Space ET_KEY_RELEASED event but do not do anything.
}

void HandleRestoreTab() {
  base::RecordAction(base::UserMetricsAction("Accel_Restore_Tab"));
  Shell::GetInstance()->new_window_delegate()->RestoreTab();
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
void HandleRotateScreen() {
  if (Shell::GetInstance()->display_manager()->IsInUnifiedMode())
    return;

  base::RecordAction(UserMetricsAction("Accel_Rotate_Window"));
  gfx::Point point = gfx::Screen::GetScreen()->GetCursorScreenPoint();
  gfx::Display display =
      gfx::Screen::GetScreen()->GetDisplayNearestPoint(point);
  const DisplayInfo& display_info =
      Shell::GetInstance()->display_manager()->GetDisplayInfo(display.id());
  ash::ScreenRotationAnimator(display.id())
      .Rotate(GetNextRotation(display_info.GetActiveRotation()),
              gfx::Display::ROTATION_SOURCE_USER);
}

// Rotate the active window.
void HandleRotateActiveWindow() {
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
            new ash::WindowRotation(360, active_window->layer())));
  }
}

void HandleShowKeyboardOverlay() {
  base::RecordAction(UserMetricsAction("Accel_Show_Keyboard_Overlay"));
  ash::Shell::GetInstance()->new_window_delegate()->ShowKeyboardOverlay();
}

bool CanHandleShowMessageCenterBubble() {
  RootWindowController* controller =
      RootWindowController::ForTargetRootWindow();
  StatusAreaWidget* status_area_widget =
      controller->shelf()->status_area_widget();
  return status_area_widget &&
         status_area_widget->web_notification_tray()->visible();
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

void HandleShowSystemTrayBubble() {
  base::RecordAction(UserMetricsAction("Accel_Show_System_Tray_Bubble"));
  RootWindowController* controller =
      RootWindowController::ForTargetRootWindow();
  if (!controller->GetSystemTray()->HasSystemBubble())
    controller->GetSystemTray()->ShowDefaultView(BUBBLE_CREATE_NEW);
}

void HandleShowTaskManager() {
  base::RecordAction(UserMetricsAction("Accel_Show_Task_Manager"));
  Shell::GetInstance()->new_window_delegate()->ShowTaskManager();
}

bool CanHandleSwitchIme(ImeControlDelegate* ime_control_delegate,
                        const ui::Accelerator& accelerator) {
  return ime_control_delegate &&
      ime_control_delegate->CanSwitchIme(accelerator);
}

void HandleSwitchIme(ImeControlDelegate* ime_control_delegate,
                     const ui::Accelerator& accelerator) {
  base::RecordAction(UserMetricsAction("Accel_Switch_Ime"));
  ime_control_delegate->HandleSwitchIme(accelerator);
}

void HandleTakePartialScreenshot(ScreenshotDelegate* screenshot_delegate) {
  base::RecordAction(UserMetricsAction("Accel_Take_Partial_Screenshot"));
  DCHECK(screenshot_delegate);
  Shell::GetInstance()
      ->partial_screenshot_controller()
      ->StartPartialScreenshotSession(screenshot_delegate);
}

void HandleTakeScreenshot(ScreenshotDelegate* screenshot_delegate) {
  base::RecordAction(UserMetricsAction("Accel_Take_Screenshot"));
  DCHECK(screenshot_delegate);
  if (screenshot_delegate->CanTakeScreenshot())
    screenshot_delegate->HandleTakeScreenshotForAllRootWindows();
}

bool CanHandleToggleAppList(const ui::Accelerator& accelerator,
                            const ui::Accelerator& previous_accelerator) {
  if (accelerator.key_code() == ui::VKEY_LWIN) {
    // If something else was pressed between the Search key (LWIN)
    // being pressed and released, then ignore the release of the
    // Search key.
    if (previous_accelerator.type() != ui::ET_KEY_PRESSED ||
        previous_accelerator.key_code() != ui::VKEY_LWIN) {
      return false;
    }

    // When spoken feedback is enabled, we should neither toggle the list nor
    // consume the key since Search+Shift is one of the shortcuts the a11y
    // feature uses. crbug.com/132296
    if (Shell::GetInstance()
            ->accessibility_delegate()
            ->IsSpokenFeedbackEnabled()) {
      return false;
    }
  }
  return true;
}

void HandleToggleAppList(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_LWIN)
    base::RecordAction(base::UserMetricsAction("Accel_Search_LWin"));
  ash::Shell::GetInstance()->ToggleAppList(NULL);
}

void HandleToggleFullscreen(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_MEDIA_LAUNCH_APP2)
    base::RecordAction(UserMetricsAction("Accel_Fullscreen_F4"));
  accelerators::ToggleFullscreen();
}

void HandleToggleOverview() {
  base::RecordAction(base::UserMetricsAction("Accel_Overview_F5"));
  Shell::GetInstance()->window_selector_controller()->ToggleOverview();
}

bool CanHandleWindowSnapOrDock() {
  wm::WindowState* window_state = wm::GetActiveWindowState();
  // Disable window snapping shortcut key for full screen window due to
  // http://crbug.com/135487.
  return (window_state && window_state->IsUserPositionable() &&
          !window_state->IsFullscreen());
}

void HandleWindowSnapOrDock(AcceleratorAction action) {
  if (action == WINDOW_CYCLE_SNAP_DOCK_LEFT)
    base::RecordAction(UserMetricsAction("Accel_Window_Snap_Left"));
  else
    base::RecordAction(UserMetricsAction("Accel_Window_Snap_Right"));

  const wm::WMEvent event(action == WINDOW_CYCLE_SNAP_DOCK_LEFT ?
                          wm::WM_EVENT_CYCLE_SNAP_DOCK_LEFT :
                          wm::WM_EVENT_CYCLE_SNAP_DOCK_RIGHT);
  wm::GetActiveWindowState()->OnWMEvent(&event);
}

void HandleWindowMinimize() {
  base::RecordAction(
      base::UserMetricsAction("Accel_Toggle_Minimized_Minus"));
  accelerators::ToggleMinimized();
}

bool CanHandlePositionCenter() {
  // Docked windows do not support centering.
  wm::WindowState* window_state = wm::GetActiveWindowState();
  return (window_state && !window_state->IsDocked());
}

void HandlePositionCenter() {
  base::RecordAction(UserMetricsAction("Accel_Window_Position_Center"));
  wm::CenterWindow(wm::GetActiveWindow());
}

#if defined(OS_CHROMEOS)
void HandleBrightnessDown(BrightnessControlDelegate* delegate,
                          const ui::Accelerator& accelerator) {
  if (delegate)
    delegate->HandleBrightnessDown(accelerator);
}

void HandleBrightnessUp(BrightnessControlDelegate* delegate,
                        const ui::Accelerator& accelerator) {
  if (delegate)
    delegate->HandleBrightnessUp(accelerator);
}

bool CanHandleDisableCapsLock(const ui::Accelerator& previous_accelerator) {
  ui::KeyboardCode previous_key_code = previous_accelerator.key_code();
  if (previous_accelerator.type() == ui::ET_KEY_RELEASED ||
      (previous_key_code != ui::VKEY_LSHIFT &&
       previous_key_code != ui::VKEY_SHIFT &&
       previous_key_code != ui::VKEY_RSHIFT)) {
    // If something else was pressed between the Shift key being pressed
    // and released, then ignore the release of the Shift key.
    return false;
  }
  chromeos::input_method::InputMethodManager* ime =
      chromeos::input_method::InputMethodManager::Get();
  chromeos::input_method::ImeKeyboard* keyboard =
      ime ? ime->GetImeKeyboard() : NULL;
  return (keyboard && keyboard->CapsLockIsEnabled());
}

void HandleDisableCapsLock() {
  base::RecordAction(UserMetricsAction("Accel_Disable_Caps_Lock"));
  chromeos::input_method::InputMethodManager* ime =
      chromeos::input_method::InputMethodManager::Get();
  ime->GetImeKeyboard()->SetCapsLockEnabled(false);
}

void HandleKeyboardBrightnessDown(KeyboardBrightnessControlDelegate* delegate,
                                  const ui::Accelerator& accelerator) {
  if (delegate)
    delegate->HandleKeyboardBrightnessDown(accelerator);
}

void HandleKeyboardBrightnessUp(KeyboardBrightnessControlDelegate* delegate,
                                const ui::Accelerator& accelerator) {
  if (delegate)
    delegate->HandleKeyboardBrightnessUp(accelerator);
}

bool CanHandleLock() {
  return Shell::GetInstance()->session_state_delegate()->CanLockScreen();
}

void HandleLock() {
  base::RecordAction(UserMetricsAction("Accel_LockScreen_L"));
  Shell::GetInstance()->session_state_delegate()->LockScreen();
}

void HandleCrosh() {
  base::RecordAction(UserMetricsAction("Accel_Open_Crosh"));

  Shell::GetInstance()->new_window_delegate()->OpenCrosh();
}

void HandleFileManager() {
  base::RecordAction(UserMetricsAction("Accel_Open_File_Manager"));

  Shell::GetInstance()->new_window_delegate()->OpenFileManager();
}

void HandleGetHelp() {
  Shell::GetInstance()->new_window_delegate()->OpenGetHelp();
}

bool CanHandleSilenceSpokenFeedback() {
  AccessibilityDelegate* delegate =
      Shell::GetInstance()->accessibility_delegate();
  return delegate->IsSpokenFeedbackEnabled();
}

void HandleSilenceSpokenFeedback() {
  base::RecordAction(UserMetricsAction("Accel_Silence_Spoken_Feedback"));
  Shell::GetInstance()->accessibility_delegate()->SilenceSpokenFeedback();
}

void HandleSwapPrimaryDisplay() {
  base::RecordAction(UserMetricsAction("Accel_Swap_Primary_Display"));
  Shell::GetInstance()->display_configuration_controller()->SetPrimaryDisplayId(
      ScreenUtil::GetSecondaryDisplay().id(), true /* user_action */);
}

bool CanHandleCycleUser() {
  Shell* shell = Shell::GetInstance();
  return shell->delegate()->IsMultiProfilesEnabled() &&
      shell->session_state_delegate()->NumberOfLoggedInUsers() > 1;
}

void HandleCycleUser(SessionStateDelegate::CycleUser cycle_user) {
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
  Shell::GetInstance()->session_state_delegate()->CycleActiveUser(cycle_user);
}

bool CanHandleToggleCapsLock(const ui::Accelerator& accelerator,
                             const ui::Accelerator& previous_accelerator) {
  if (accelerator.key_code() == ui::VKEY_LWIN) {
    // If something else was pressed between the Search key (LWIN)
    // being pressed and released, then ignore the release of the
    // Search key.
    // TODO(danakj): Releasing Alt first breaks this: crbug.com/166495
    if (previous_accelerator.type() == ui::ET_KEY_RELEASED ||
        previous_accelerator.key_code() != ui::VKEY_LWIN)
      return false;
  }
  chromeos::input_method::InputMethodManager* ime =
      chromeos::input_method::InputMethodManager::Get();
  return ime && ime->GetImeKeyboard();
}

void HandleToggleCapsLock() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Caps_Lock"));
  chromeos::input_method::InputMethodManager* ime =
      chromeos::input_method::InputMethodManager::Get();
  chromeos::input_method::ImeKeyboard* keyboard = ime->GetImeKeyboard();
  keyboard->SetCapsLockEnabled(!keyboard->CapsLockIsEnabled());
}

void HandleToggleMirrorMode() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Mirror_Mode"));
  bool mirror = !Shell::GetInstance()->display_manager()->IsInMirrorMode();
  Shell::GetInstance()->display_configuration_controller()->SetMirrorMode(
      mirror, true /* user_action */);
}

void HandleToggleSpokenFeedback() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Spoken_Feedback"));

  Shell::GetInstance()->accessibility_delegate()->
      ToggleSpokenFeedback(ui::A11Y_NOTIFICATION_SHOW);
}

bool CanHandleToggleTouchViewTesting() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshEnableTouchViewTesting);
}

void HandleToggleTouchViewTesting() {
  // TODO(skuhne): This is only temporary! Remove this!
  MaximizeModeController* controller = Shell::GetInstance()->
      maximize_mode_controller();
  controller->EnableMaximizeModeWindowManager(
      !controller->IsMaximizeModeWindowManagerEnabled());
}

bool CanHandleTouchHud() {
  return RootWindowController::ForTargetRootWindow()->touch_hud_debug();
}

void HandleTouchHudClear() {
  RootWindowController::ForTargetRootWindow()->touch_hud_debug()->Clear();
}

void HandleTouchHudModeChange() {
  RootWindowController* controller =
      RootWindowController::ForTargetRootWindow();
  controller->touch_hud_debug()->ChangeToNextMode();
}

void HandleVolumeDown(const ui::Accelerator& accelerator) {
  VolumeControlDelegate* volume_delegate =
      Shell::GetInstance()->system_tray_delegate()->GetVolumeControlDelegate();
  if (volume_delegate)
    volume_delegate->HandleVolumeDown(accelerator);
}

void HandleVolumeMute(const ui::Accelerator& accelerator) {
  VolumeControlDelegate* volume_delegate =
      Shell::GetInstance()->system_tray_delegate()->GetVolumeControlDelegate();
  if (volume_delegate)
    volume_delegate->HandleVolumeMute(accelerator);
}

void HandleVolumeUp(const ui::Accelerator& accelerator) {
  VolumeControlDelegate* volume_delegate =
      Shell::GetInstance()->system_tray_delegate()->GetVolumeControlDelegate();
  if (volume_delegate)
    volume_delegate->HandleVolumeUp(accelerator);
}

#endif  // defined(OS_CHROMEOS)

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AcceleratorController, public:

AcceleratorController::AcceleratorController()
    : accelerator_manager_(new ui::AcceleratorManager),
      accelerator_history_(new ui::AcceleratorHistory) {
  Init();
}

AcceleratorController::~AcceleratorController() {
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
  if (ime_control_delegate_)
    return accelerator_manager_->Process(accelerator);
  return accelerator_manager_->Process(accelerator);
}

bool AcceleratorController::IsRegistered(
    const ui::Accelerator& accelerator) const {
  return accelerator_manager_->GetCurrentTarget(accelerator) != NULL;
}

bool AcceleratorController::IsPreferred(
    const ui::Accelerator& accelerator) const {
  std::map<ui::Accelerator, AcceleratorAction>::const_iterator iter =
      accelerators_.find(accelerator);
  if (iter == accelerators_.end())
    return false;  // not an accelerator.

  return preferred_actions_.find(iter->second) != preferred_actions_.end();
}

bool AcceleratorController::IsReserved(
    const ui::Accelerator& accelerator) const {
  std::map<ui::Accelerator, AcceleratorAction>::const_iterator iter =
      accelerators_.find(accelerator);
  if (iter == accelerators_.end())
    return false;  // not an accelerator.

  return reserved_actions_.find(iter->second) != reserved_actions_.end();
}

bool AcceleratorController::IsDeprecated(
    const ui::Accelerator& accelerator) const {
  return deprecated_accelerators_.count(accelerator) != 0;
}

bool AcceleratorController::PerformActionIfEnabled(AcceleratorAction action) {
  if (CanPerformAction(action, ui::Accelerator())) {
    PerformAction(action, ui::Accelerator());
    return true;
  }
  return false;
}

AcceleratorController::AcceleratorProcessingRestriction
AcceleratorController::GetCurrentAcceleratorRestriction() {
  return GetAcceleratorProcessingRestriction(-1);
}

void AcceleratorController::SetBrightnessControlDelegate(
    scoped_ptr<BrightnessControlDelegate> brightness_control_delegate) {
  brightness_control_delegate_ = std::move(brightness_control_delegate);
}

void AcceleratorController::SetImeControlDelegate(
    scoped_ptr<ImeControlDelegate> ime_control_delegate) {
  ime_control_delegate_ = std::move(ime_control_delegate);
}

void AcceleratorController::SetScreenshotDelegate(
    scoped_ptr<ScreenshotDelegate> screenshot_delegate) {
  screenshot_delegate_ = std::move(screenshot_delegate);
}

bool AcceleratorController::ShouldCloseMenuAndRepostAccelerator(
    const ui::Accelerator& accelerator) const {
  auto itr = accelerators_.find(accelerator);
  if (itr == accelerators_.end())
    return false;  // Menu shouldn't be closed for an invalid accelerator.

  AcceleratorAction action = itr->second;
  return actions_keeping_menu_open_.count(action) == 0;
}

////////////////////////////////////////////////////////////////////////////////
// AcceleratorController, ui::AcceleratorTarget implementation:

bool AcceleratorController::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  std::map<ui::Accelerator, AcceleratorAction>::const_iterator it =
      accelerators_.find(accelerator);
  DCHECK(it != accelerators_.end());
  AcceleratorAction action = it->second;
  if (CanPerformAction(action, accelerator)) {
    // Handling the deprecated accelerators (if any) only if action can be
    // performed.
    auto itr = actions_with_deprecations_.find(action);
    if (itr != actions_with_deprecations_.end()) {
      const DeprecatedAcceleratorData* data = itr->second;
      if (deprecated_accelerators_.count(accelerator)) {
        // This accelerator has been deprecated and should be treated according
        // to its |DeprecatedAcceleratorData|.

        // Record UMA stats.
        RecordUmaHistogram(data->uma_histogram_name, DEPRECATED_USED);

        // We always display the notification as long as this entry exists.
        ShowDeprecatedAcceleratorNotification(
            data->uma_histogram_name, data->notification_message_id,
            data->old_shortcut_id, data->new_shortcut_id);

        if (!data->deprecated_enabled)
          return false;
      } else {
        // This is a new accelerator replacing the old deprecated one.
        // Record UMA stats and proceed normally.
        RecordUmaHistogram(data->uma_histogram_name, NEW_USED);
      }
    }

    PerformAction(action, accelerator);
    return ShouldActionConsumeKeyEvent(action);
  }
  return false;
}

bool AcceleratorController::CanHandleAccelerators() const {
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// AcceleratorController, private:

void AcceleratorController::Init() {
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
  for (size_t i = 0; i < kActionsKeepingMenuOpenLength; ++i)
    actions_keeping_menu_open_.insert(kActionsKeepingMenuOpen[i]);

  RegisterAccelerators(kAcceleratorData, kAcceleratorDataLength);

  RegisterDeprecatedAccelerators();

  if (debug::DebugAcceleratorsEnabled()) {
    RegisterAccelerators(kDebugAcceleratorData, kDebugAcceleratorDataLength);
    // All debug accelerators are reserved.
    for (size_t i = 0; i < kDebugAcceleratorDataLength; ++i)
      reserved_actions_.insert(kDebugAcceleratorData[i].action);
  }

#if defined(OS_CHROMEOS)
  keyboard_brightness_control_delegate_.reset(
      new KeyboardBrightnessController());
#endif
}

void AcceleratorController::RegisterAccelerators(
    const AcceleratorData accelerators[],
    size_t accelerators_length) {
  for (size_t i = 0; i < accelerators_length; ++i) {
    ui::Accelerator accelerator =
        CreateAccelerator(accelerators[i].keycode, accelerators[i].modifiers,
                          accelerators[i].trigger_on_press);
    Register(accelerator, this);
    accelerators_.insert(
        std::make_pair(accelerator, accelerators[i].action));
  }
}

void AcceleratorController::RegisterDeprecatedAccelerators() {
#if defined(OS_CHROMEOS)
  for (size_t i = 0; i < kDeprecatedAcceleratorsDataLength; ++i) {
    const DeprecatedAcceleratorData* data = &kDeprecatedAcceleratorsData[i];
    actions_with_deprecations_[data->action] = data;
  }

  for (size_t i = 0; i < kDeprecatedAcceleratorsLength; ++i) {
    const AcceleratorData& accelerator_data = kDeprecatedAccelerators[i];
    const ui::Accelerator deprecated_accelerator =
        CreateAccelerator(accelerator_data.keycode, accelerator_data.modifiers,
                          accelerator_data.trigger_on_press);

    Register(deprecated_accelerator, this);
    accelerators_[deprecated_accelerator] = accelerator_data.action;
    deprecated_accelerators_.insert(deprecated_accelerator);
  }
#endif  // defined(OS_CHROMEOS)
}

bool AcceleratorController::CanPerformAction(
    AcceleratorAction action,
    const ui::Accelerator& accelerator) {
  if (nonrepeatable_actions_.find(action) != nonrepeatable_actions_.end() &&
      accelerator.IsRepeat()) {
    return false;
  }

  AcceleratorProcessingRestriction restriction =
      GetAcceleratorProcessingRestriction(action);
  if (restriction != RESTRICTION_NONE)
    return restriction == RESTRICTION_PREVENT_PROCESSING_AND_PROPAGATION;

  const ui::Accelerator& previous_accelerator =
      accelerator_history_->previous_accelerator();

  // True should be returned if running |action| does something. Otherwise,
  // false should be returned to give the web contents a chance at handling the
  // accelerator.
  switch (action) {
    case DEBUG_PRINT_LAYER_HIERARCHY:
    case DEBUG_PRINT_VIEW_HIERARCHY:
    case DEBUG_PRINT_WINDOW_HIERARCHY:
    case DEBUG_TOGGLE_DESKTOP_BACKGROUND_MODE:
    case DEBUG_TOGGLE_DEVICE_SCALE_FACTOR:
    case DEBUG_TOGGLE_ROOT_WINDOW_FULL_SCREEN:
    case DEBUG_TOGGLE_SHOW_DEBUG_BORDERS:
    case DEBUG_TOGGLE_SHOW_FPS_COUNTER:
    case DEBUG_TOGGLE_SHOW_PAINT_RECTS:
      return debug::DebugAcceleratorsEnabled();
    case MAGNIFY_SCREEN_ZOOM_IN:
    case MAGNIFY_SCREEN_ZOOM_OUT:
      return CanHandleMagnifyScreen();
    case NEW_INCOGNITO_WINDOW:
      return CanHandleNewIncognitoWindow();
    case NEXT_IME: {
#if defined(OS_CHROMEOS)
      bool accelerator_is_deprecated =
          (deprecated_accelerators_.count(accelerator) != 0);
#else
      // On non-chromeos, the NEXT_IME deprecated accelerators are always used.
      bool accelerator_is_deprecated = true;
#endif  // defined(OS_CHROMEOS)

      return CanHandleNextIme(ime_control_delegate_.get(), previous_accelerator,
                              accelerator_is_deprecated);
    }
    case PREVIOUS_IME:
      return CanHandlePreviousIme(ime_control_delegate_.get());
    case SCALE_UI_RESET:
    case SCALE_UI_UP:
    case SCALE_UI_DOWN:
      return accelerators::IsInternalDisplayZoomEnabled();
    case SHOW_MESSAGE_CENTER_BUBBLE:
      return CanHandleShowMessageCenterBubble();
    case SWITCH_IME:
      return CanHandleSwitchIme(ime_control_delegate_.get(), accelerator);
    case TOGGLE_APP_LIST:
      return CanHandleToggleAppList(accelerator, previous_accelerator);
    case WINDOW_CYCLE_SNAP_DOCK_LEFT:
    case WINDOW_CYCLE_SNAP_DOCK_RIGHT:
      return CanHandleWindowSnapOrDock();
    case WINDOW_POSITION_CENTER:
      return CanHandlePositionCenter();
#if defined(OS_CHROMEOS)
    case DEBUG_ADD_REMOVE_DISPLAY:
    case DEBUG_TOGGLE_TOUCH_PAD:
    case DEBUG_TOGGLE_TOUCH_SCREEN:
    case DEBUG_TOGGLE_UNIFIED_DESKTOP:
      return debug::DebugAcceleratorsEnabled();
    case DISABLE_CAPS_LOCK:
      return CanHandleDisableCapsLock(previous_accelerator);
    case LOCK_SCREEN:
      return CanHandleLock();
    case SILENCE_SPOKEN_FEEDBACK:
      return CanHandleSilenceSpokenFeedback();
    case SWITCH_TO_PREVIOUS_USER:
    case SWITCH_TO_NEXT_USER:
      return CanHandleCycleUser();
    case TOGGLE_CAPS_LOCK:
      return CanHandleToggleCapsLock(accelerator, previous_accelerator);
    case TOGGLE_TOUCH_VIEW_TESTING:
      return CanHandleToggleTouchViewTesting();
    case TOUCH_HUD_CLEAR:
    case TOUCH_HUD_MODE_CHANGE:
      return CanHandleTouchHud();
#endif

    case CYCLE_BACKWARD_MRU:
    case CYCLE_FORWARD_MRU:
    case EXIT:
    case FOCUS_NEXT_PANE:
    case FOCUS_PREVIOUS_PANE:
    case FOCUS_SHELF:
    case LAUNCH_APP_0:
    case LAUNCH_APP_1:
    case LAUNCH_APP_2:
    case LAUNCH_APP_3:
    case LAUNCH_APP_4:
    case LAUNCH_APP_5:
    case LAUNCH_APP_6:
    case LAUNCH_APP_7:
    case LAUNCH_LAST_APP:
    case MEDIA_NEXT_TRACK:
    case MEDIA_PLAY_PAUSE:
    case MEDIA_PREV_TRACK:
    case NEW_TAB:
    case NEW_WINDOW:
    case OPEN_FEEDBACK_PAGE:
    case PRINT_UI_HIERARCHIES:
    case RESTORE_TAB:
    case ROTATE_SCREEN:
    case ROTATE_WINDOW:
    case SHOW_KEYBOARD_OVERLAY:
    case SHOW_SYSTEM_TRAY_BUBBLE:
    case SHOW_TASK_MANAGER:
    case TAKE_PARTIAL_SCREENSHOT:
    case TAKE_SCREENSHOT:
    case TOGGLE_FULLSCREEN:
    case TOGGLE_MAXIMIZED:
    case TOGGLE_OVERVIEW:
    case WINDOW_MINIMIZE:
#if defined(OS_CHROMEOS)
    case BRIGHTNESS_DOWN:
    case BRIGHTNESS_UP:
    case DISABLE_GPU_WATCHDOG:
    case KEYBOARD_BRIGHTNESS_DOWN:
    case KEYBOARD_BRIGHTNESS_UP:
    case LOCK_PRESSED:
    case LOCK_RELEASED:
    case OPEN_CROSH:
    case OPEN_FILE_MANAGER:
    case OPEN_GET_HELP:
    case POWER_PRESSED:
    case POWER_RELEASED:
    case SWAP_PRIMARY_DISPLAY:
    case TOGGLE_MIRROR_MODE:
    case TOGGLE_SPOKEN_FEEDBACK:
    case TOGGLE_WIFI:
    case TOUCH_HUD_PROJECTION_TOGGLE:
    case VOLUME_DOWN:
    case VOLUME_MUTE:
    case VOLUME_UP:
#else
    case DUMMY_FOR_RESERVED:
#endif
      return true;
  }
  return false;
}

void AcceleratorController::PerformAction(AcceleratorAction action,
                                          const ui::Accelerator& accelerator) {
  AcceleratorProcessingRestriction restriction =
      GetAcceleratorProcessingRestriction(action);
  if (restriction != RESTRICTION_NONE)
    return;

  // If your accelerator invokes more than one line of code, please either
  // implement it in your module's controller code (like TOGGLE_MIRROR_MODE
  // below) or pull it into a HandleFoo() function above.
  switch (action) {
    case CYCLE_BACKWARD_MRU:
      HandleCycleBackwardMRU(accelerator);
      break;
    case CYCLE_FORWARD_MRU:
      HandleCycleForwardMRU(accelerator);
      break;
    case DEBUG_PRINT_LAYER_HIERARCHY:
    case DEBUG_PRINT_VIEW_HIERARCHY:
    case DEBUG_PRINT_WINDOW_HIERARCHY:
    case DEBUG_TOGGLE_DESKTOP_BACKGROUND_MODE:
    case DEBUG_TOGGLE_DEVICE_SCALE_FACTOR:
    case DEBUG_TOGGLE_ROOT_WINDOW_FULL_SCREEN:
    case DEBUG_TOGGLE_SHOW_DEBUG_BORDERS:
    case DEBUG_TOGGLE_SHOW_FPS_COUNTER:
    case DEBUG_TOGGLE_SHOW_PAINT_RECTS:
      debug::PerformDebugActionIfEnabled(action);
      break;
    case EXIT:
      // UMA metrics are recorded in the handler.
      exit_warning_handler_.HandleAccelerator();
      break;
    case FOCUS_NEXT_PANE:
      HandleRotatePaneFocus(Shell::FORWARD);
      break;
    case FOCUS_PREVIOUS_PANE:
      HandleRotatePaneFocus(Shell::BACKWARD);
      break;
    case FOCUS_SHELF:
      HandleFocusShelf();
      break;
    case LAUNCH_APP_0:
      HandleLaunchAppN(0);
      break;
    case LAUNCH_APP_1:
      HandleLaunchAppN(1);
      break;
    case LAUNCH_APP_2:
      HandleLaunchAppN(2);
      break;
    case LAUNCH_APP_3:
      HandleLaunchAppN(3);
      break;
    case LAUNCH_APP_4:
      HandleLaunchAppN(4);
      break;
    case LAUNCH_APP_5:
      HandleLaunchAppN(5);
      break;
    case LAUNCH_APP_6:
      HandleLaunchAppN(6);
      break;
    case LAUNCH_APP_7:
      HandleLaunchAppN(7);
      break;
    case LAUNCH_LAST_APP:
      HandleLaunchLastApp();
      break;
    case MAGNIFY_SCREEN_ZOOM_IN:
      HandleMagnifyScreen(1);
      break;
    case MAGNIFY_SCREEN_ZOOM_OUT:
      HandleMagnifyScreen(-1);
      break;
    case MEDIA_NEXT_TRACK:
      HandleMediaNextTrack();
      break;
    case MEDIA_PLAY_PAUSE:
      HandleMediaPlayPause();
      break;
    case MEDIA_PREV_TRACK:
      HandleMediaPrevTrack();
      break;
    case NEW_INCOGNITO_WINDOW:
      HandleNewIncognitoWindow();
      break;
    case NEW_TAB:
      HandleNewTab(accelerator);
      break;
    case NEW_WINDOW:
      HandleNewWindow();
      break;
    case NEXT_IME:
      HandleNextIme(ime_control_delegate_.get());
      break;
    case OPEN_FEEDBACK_PAGE:
      HandleOpenFeedbackPage();
      break;
    case PREVIOUS_IME:
      HandlePreviousIme(ime_control_delegate_.get(), accelerator);
      break;
    case PRINT_UI_HIERARCHIES:
      debug::PrintUIHierarchies();
      break;
    case RESTORE_TAB:
      HandleRestoreTab();
      break;
    case ROTATE_SCREEN:
      HandleRotateScreen();
      break;
    case ROTATE_WINDOW:
      HandleRotateActiveWindow();
      break;
    case SCALE_UI_DOWN:
      accelerators::ZoomInternalDisplay(false /* down */);
      break;
    case SCALE_UI_RESET:
      accelerators::ResetInternalDisplayZoom();
      break;
    case SCALE_UI_UP:
      accelerators::ZoomInternalDisplay(true /* up */);
      break;
    case SHOW_KEYBOARD_OVERLAY:
      HandleShowKeyboardOverlay();
      break;
    case SHOW_MESSAGE_CENTER_BUBBLE:
      HandleShowMessageCenterBubble();
      break;
    case SHOW_SYSTEM_TRAY_BUBBLE:
      HandleShowSystemTrayBubble();
      break;
    case SHOW_TASK_MANAGER:
      HandleShowTaskManager();
      break;
    case SWITCH_IME:
      HandleSwitchIme(ime_control_delegate_.get(), accelerator);
      break;
    case TAKE_PARTIAL_SCREENSHOT:
      HandleTakePartialScreenshot(screenshot_delegate_.get());
      break;
    case TAKE_SCREENSHOT:
      HandleTakeScreenshot(screenshot_delegate_.get());
      break;
    case TOGGLE_APP_LIST:
      HandleToggleAppList(accelerator);
      break;
    case TOGGLE_FULLSCREEN:
      HandleToggleFullscreen(accelerator);
      break;
    case TOGGLE_MAXIMIZED:
      accelerators::ToggleMaximized();
      break;
    case TOGGLE_OVERVIEW:
      HandleToggleOverview();
      break;
    case WINDOW_CYCLE_SNAP_DOCK_LEFT:
    case WINDOW_CYCLE_SNAP_DOCK_RIGHT:
      HandleWindowSnapOrDock(action);
      break;
    case WINDOW_MINIMIZE:
      HandleWindowMinimize();
      break;
    case WINDOW_POSITION_CENTER:
      HandlePositionCenter();
      break;
#if defined(OS_CHROMEOS)
    case BRIGHTNESS_DOWN:
      HandleBrightnessDown(brightness_control_delegate_.get(), accelerator);
      break;
    case BRIGHTNESS_UP:
      HandleBrightnessUp(brightness_control_delegate_.get(), accelerator);
      break;
    case DEBUG_ADD_REMOVE_DISPLAY:
    case DEBUG_TOGGLE_TOUCH_PAD:
    case DEBUG_TOGGLE_TOUCH_SCREEN:
    case DEBUG_TOGGLE_UNIFIED_DESKTOP:
      debug::PerformDebugActionIfEnabled(action);
      break;
    case DISABLE_CAPS_LOCK:
      HandleDisableCapsLock();
      break;
    case DISABLE_GPU_WATCHDOG:
      Shell::GetInstance()->gpu_support()->DisableGpuWatchdog();
      break;
    case KEYBOARD_BRIGHTNESS_DOWN:
      HandleKeyboardBrightnessDown(keyboard_brightness_control_delegate_.get(),
                                   accelerator);
      break;
    case KEYBOARD_BRIGHTNESS_UP:
      HandleKeyboardBrightnessUp(keyboard_brightness_control_delegate_.get(),
                                 accelerator);
      break;
    case LOCK_PRESSED:
    case LOCK_RELEASED:
      Shell::GetInstance()->power_button_controller()->
          OnLockButtonEvent(action == LOCK_PRESSED, base::TimeTicks());
      break;
    case LOCK_SCREEN:
      HandleLock();
      break;
    case OPEN_CROSH:
      HandleCrosh();
      break;
    case OPEN_FILE_MANAGER:
      HandleFileManager();
      break;
    case OPEN_GET_HELP:
      HandleGetHelp();
      break;
    case POWER_PRESSED:  // fallthrough
    case POWER_RELEASED:
      if (!base::SysInfo::IsRunningOnChromeOS()) {
        // There is no powerd, the Chrome OS power manager, in linux desktop,
        // so call the PowerButtonController here.
        Shell::GetInstance()->power_button_controller()->
            OnPowerButtonEvent(action == POWER_PRESSED, base::TimeTicks());
      }
      // We don't do anything with these at present on the device,
      // (power button events are reported to us from powerm via
      // D-BUS), but we consume them to prevent them from getting
      // passed to apps -- see http://crbug.com/146609.
      break;
    case SILENCE_SPOKEN_FEEDBACK:
      HandleSilenceSpokenFeedback();
      break;
    case SWAP_PRIMARY_DISPLAY:
      HandleSwapPrimaryDisplay();
      break;
    case SWITCH_TO_NEXT_USER:
      HandleCycleUser(SessionStateDelegate::CYCLE_TO_NEXT_USER);
      break;
    case SWITCH_TO_PREVIOUS_USER:
      HandleCycleUser(SessionStateDelegate::CYCLE_TO_PREVIOUS_USER);
      break;
    case TOGGLE_CAPS_LOCK:
      HandleToggleCapsLock();
      break;
    case TOGGLE_MIRROR_MODE:
      HandleToggleMirrorMode();
      break;
    case TOGGLE_SPOKEN_FEEDBACK:
      HandleToggleSpokenFeedback();
      break;
    case TOGGLE_TOUCH_VIEW_TESTING:
      HandleToggleTouchViewTesting();
      break;
    case TOGGLE_WIFI:
      Shell::GetInstance()->system_tray_notifier()->NotifyRequestToggleWifi();
      break;
    case TOUCH_HUD_CLEAR:
      HandleTouchHudClear();
      break;
    case TOUCH_HUD_MODE_CHANGE:
      HandleTouchHudModeChange();
      break;
    case TOUCH_HUD_PROJECTION_TOGGLE:
      accelerators::ToggleTouchHudProjection();
      break;
    case VOLUME_DOWN:
      HandleVolumeDown(accelerator);
      break;
    case VOLUME_MUTE:
      HandleVolumeMute(accelerator);
      break;
    case VOLUME_UP:
      HandleVolumeUp(accelerator);
      break;
#else
    case DUMMY_FOR_RESERVED:
      NOTREACHED();
      break;
#endif
  }
}

bool AcceleratorController::ShouldActionConsumeKeyEvent(
    AcceleratorAction action) {
#if defined(OS_CHROMEOS)
  if (action == SILENCE_SPOKEN_FEEDBACK)
    return false;
#endif

  // Adding new exceptions is *STRONGLY* discouraged.
  return true;
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
  if (shell->delegate()->IsRunningInForcedAppMode() &&
      actions_allowed_in_app_mode_.find(action) ==
          actions_allowed_in_app_mode_.end()) {
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
  if (shell->mru_window_tracker()->BuildMruWindowList().empty() &&
      actions_needing_window_.find(action) != actions_needing_window_.end()) {
    Shell::GetInstance()->accessibility_delegate()->TriggerAccessibilityAlert(
        ui::A11Y_ALERT_WINDOW_NEEDED);
    return RESTRICTION_PREVENT_PROCESSING_AND_PROPAGATION;
  }
  return RESTRICTION_NONE;
}

void AcceleratorController::SetKeyboardBrightnessControlDelegate(
    scoped_ptr<KeyboardBrightnessControlDelegate>
    keyboard_brightness_control_delegate) {
  keyboard_brightness_control_delegate_ =
      std::move(keyboard_brightness_control_delegate);
}

}  // namespace ash
