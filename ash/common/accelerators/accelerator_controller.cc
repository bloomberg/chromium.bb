// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/accelerators/accelerator_controller.h"

#include <utility>

#include "ash/common/accelerators/accelerator_commands.h"
#include "ash/common/accelerators/accelerator_controller_delegate.h"
#include "ash/common/accelerators/debug_commands.h"
#include "ash/common/accessibility_delegate.h"
#include "ash/common/accessibility_types.h"
#include "ash/common/focus_cycler.h"
#include "ash/common/ime_control_delegate.h"
#include "ash/common/media_controller.h"
#include "ash/common/multi_profile_uma.h"
#include "ash/common/new_window_controller.h"
#include "ash/common/palette_delegate.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/shelf_widget.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shell_delegate.h"
#include "ash/common/system/brightness_control_delegate.h"
#include "ash/common/system/chromeos/ime_menu/ime_menu_tray.h"
#include "ash/common/system/chromeos/palette/palette_tray.h"
#include "ash/common/system/chromeos/palette/palette_utils.h"
#include "ash/common/system/keyboard_brightness_control_delegate.h"
#include "ash/common/system/status_area_widget.h"
#include "ash/common/system/system_notifier.h"
#include "ash/common/system/tray/system_tray_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/system/web_notification/web_notification_tray.h"
#include "ash/common/wm/mru_window_tracker.h"
#include "ash/common/wm/overview/window_selector_controller.h"
#include "ash/common/wm/window_cycle_controller.h"
#include "ash/common/wm/window_positioning_utils.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm/wm_event.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/message_center/message_center.h"

namespace ash {
namespace {

using base::UserMetricsAction;
using message_center::Notification;

// Identifier for the high contrast toggle accelerator notification.
const char kHighContrastToggleAccelNotificationId[] =
    "chrome://settings/accessibility/highcontrast";

ui::Accelerator CreateAccelerator(ui::KeyboardCode keycode,
                                  int modifiers,
                                  bool trigger_on_press) {
  ui::Accelerator accelerator(keycode, modifiers);
  accelerator.set_type(trigger_on_press ? ui::ET_KEY_PRESSED
                                        : ui::ET_KEY_RELEASED);
  return accelerator;
}

void RecordUmaHistogram(const char* histogram_name,
                        DeprecatedAcceleratorUsage sample) {
  auto* histogram = base::LinearHistogram::FactoryGet(
      histogram_name, 1, DEPRECATED_USAGE_COUNT, DEPRECATED_USAGE_COUNT + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(sample);
}

void HandleCycleBackwardMRU(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_TAB)
    base::RecordAction(base::UserMetricsAction("Accel_PrevWindow_Tab"));

  WmShell::Get()->window_cycle_controller()->HandleCycleWindow(
      WindowCycleController::BACKWARD);
}

void HandleCycleForwardMRU(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_TAB)
    base::RecordAction(base::UserMetricsAction("Accel_NextWindow_Tab"));

  WmShell::Get()->window_cycle_controller()->HandleCycleWindow(
      WindowCycleController::FORWARD);
}

void HandleRotatePaneFocus(FocusCycler::Direction direction) {
  switch (direction) {
    // TODO(stevet): Not sure if this is the same as IDC_FOCUS_NEXT_PANE.
    case FocusCycler::FORWARD: {
      base::RecordAction(UserMetricsAction("Accel_Focus_Next_Pane"));
      break;
    }
    case FocusCycler::BACKWARD: {
      base::RecordAction(UserMetricsAction("Accel_Focus_Previous_Pane"));
      break;
    }
  }
  WmShell::Get()->focus_cycler()->RotateFocus(direction);
}

void HandleFocusShelf() {
  base::RecordAction(UserMetricsAction("Accel_Focus_Shelf"));
  // TODO(jamescook): Should this be GetRootWindowForNewWindows()?
  WmShelf* shelf = WmShelf::ForWindow(WmShell::Get()->GetPrimaryRootWindow());
  WmShell::Get()->focus_cycler()->FocusWidget(shelf->shelf_widget());
}

void HandleLaunchAppN(int n) {
  base::RecordAction(UserMetricsAction("Accel_Launch_App"));
  WmShelf::LaunchShelfItem(n);
}

void HandleLaunchLastApp() {
  base::RecordAction(UserMetricsAction("Accel_Launch_Last_App"));
  WmShelf::LaunchShelfItem(-1);
}

void HandleMediaNextTrack() {
  WmShell::Get()->media_controller()->HandleMediaNextTrack();
}

void HandleMediaPlayPause() {
  WmShell::Get()->media_controller()->HandleMediaPlayPause();
}

void HandleMediaPrevTrack() {
  WmShell::Get()->media_controller()->HandleMediaPrevTrack();
}

bool CanHandleNewIncognitoWindow() {
  return WmShell::Get()->delegate()->IsIncognitoAllowed();
}

void HandleNewIncognitoWindow() {
  base::RecordAction(UserMetricsAction("Accel_New_Incognito_Window"));
  WmShell::Get()->new_window_controller()->NewWindow(true /* is_incognito */);
}

void HandleNewTab(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_T)
    base::RecordAction(UserMetricsAction("Accel_NewTab_T"));
  WmShell::Get()->new_window_controller()->NewTab();
}

void HandleNewWindow() {
  base::RecordAction(UserMetricsAction("Accel_New_Window"));
  WmShell::Get()->new_window_controller()->NewWindow(false /* is_incognito */);
}

bool CanHandleNextIme(ImeControlDelegate* ime_control_delegate) {
  return ime_control_delegate && ime_control_delegate->CanCycleIme();
}

bool CanHandleCycleMru(const ui::Accelerator& accelerator) {
  // Don't do anything when Alt+Tab is hit while a virtual keyboard is showing.
  // Touchscreen users have better window switching options. It would be
  // preferable if we could tell whether this event actually came from a virtual
  // keyboard, but there's no easy way to do so, thus we block Alt+Tab when the
  // virtual keyboard is showing, even if it came from a real keyboard. See
  // http://crbug.com/638269
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  return !(keyboard_controller && keyboard_controller->keyboard_visible());
}

void HandleNextIme(ImeControlDelegate* ime_control_delegate) {
  base::RecordAction(UserMetricsAction("Accel_Next_Ime"));
  ime_control_delegate->HandleNextIme();
}

void HandleOpenFeedbackPage() {
  base::RecordAction(UserMetricsAction("Accel_Open_Feedback_Page"));
  WmShell::Get()->new_window_controller()->OpenFeedbackPage();
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
  base::RecordAction(UserMetricsAction("Accel_Restore_Tab"));
  WmShell::Get()->new_window_controller()->RestoreTab();
}

void HandleShowKeyboardOverlay() {
  base::RecordAction(UserMetricsAction("Accel_Show_Keyboard_Overlay"));
  WmShell::Get()->new_window_controller()->ShowKeyboardOverlay();
}

bool CanHandleShowMessageCenterBubble() {
  WmWindow* target_root = WmShell::Get()->GetRootWindowForNewWindows();
  StatusAreaWidget* status_area_widget =
      WmShelf::ForWindow(target_root)->shelf_widget()->status_area_widget();
  return status_area_widget &&
         status_area_widget->web_notification_tray()->visible();
}

void HandleShowMessageCenterBubble() {
  base::RecordAction(UserMetricsAction("Accel_Show_Message_Center_Bubble"));
  WmWindow* target_root = WmShell::Get()->GetRootWindowForNewWindows();
  StatusAreaWidget* status_area_widget =
      WmShelf::ForWindow(target_root)->shelf_widget()->status_area_widget();
  if (status_area_widget) {
    WebNotificationTray* notification_tray =
        status_area_widget->web_notification_tray();
    if (notification_tray->visible())
      notification_tray->ShowMessageCenterBubble();
  }
}

void HandleShowTaskManager() {
  base::RecordAction(UserMetricsAction("Accel_Show_Task_Manager"));
  WmShell::Get()->new_window_controller()->ShowTaskManager();
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
    if (WmShell::Get()->accessibility_delegate()->IsSpokenFeedbackEnabled())
      return false;
  }
  return true;
}

void HandleToggleAppList(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_LWIN)
    base::RecordAction(UserMetricsAction("Accel_Search_LWin"));
  WmShell::Get()->ToggleAppList();
}

void HandleToggleFullscreen(const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_MEDIA_LAUNCH_APP2)
    base::RecordAction(UserMetricsAction("Accel_Fullscreen_F4"));
  accelerators::ToggleFullscreen();
}

void HandleToggleOverview() {
  base::RecordAction(base::UserMetricsAction("Accel_Overview_F5"));
  WmShell::Get()->window_selector_controller()->ToggleOverview();
}

bool CanHandleWindowSnapOrDock() {
  WmWindow* active_window = WmShell::Get()->GetActiveWindow();
  if (!active_window)
    return false;
  wm::WindowState* window_state = active_window->GetWindowState();
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

  const wm::WMEvent event(action == WINDOW_CYCLE_SNAP_DOCK_LEFT
                              ? wm::WM_EVENT_CYCLE_SNAP_DOCK_LEFT
                              : wm::WM_EVENT_CYCLE_SNAP_DOCK_RIGHT);
  WmWindow* active_window = WmShell::Get()->GetActiveWindow();
  DCHECK(active_window);
  active_window->GetWindowState()->OnWMEvent(&event);
}

void HandleWindowMinimize() {
  base::RecordAction(base::UserMetricsAction("Accel_Toggle_Minimized_Minus"));
  accelerators::ToggleMinimized();
}

bool CanHandlePositionCenter() {
  // Docked windows do not support centering.
  WmWindow* active_window = WmShell::Get()->GetActiveWindow();
  return (active_window && !active_window->GetWindowState()->IsDocked());
}

void HandlePositionCenter() {
  base::RecordAction(UserMetricsAction("Accel_Window_Position_Center"));
  wm::CenterWindow(WmShell::Get()->GetActiveWindow());
}

void HandleShowImeMenuBubble() {
  base::RecordAction(UserMetricsAction("Accel_Show_Ime_Menu_Bubble"));

  StatusAreaWidget* status_area_widget =
      WmShelf::ForWindow(WmShell::Get()->GetPrimaryRootWindow())
          ->GetStatusAreaWidget();
  if (status_area_widget) {
    ImeMenuTray* ime_menu_tray = status_area_widget->ime_menu_tray();
    if (ime_menu_tray && ime_menu_tray->visible() &&
        !ime_menu_tray->IsImeMenuBubbleShown()) {
      ime_menu_tray->ShowImeMenuBubble();
    }
  }
}

void HandleCrosh() {
  base::RecordAction(UserMetricsAction("Accel_Open_Crosh"));

  WmShell::Get()->new_window_controller()->OpenCrosh();
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

void HandleFileManager() {
  base::RecordAction(UserMetricsAction("Accel_Open_File_Manager"));

  WmShell::Get()->new_window_controller()->OpenFileManager();
}

void HandleGetHelp() {
  WmShell::Get()->new_window_controller()->OpenGetHelp();
}

bool CanHandleLock() {
  return WmShell::Get()->GetSessionStateDelegate()->CanLockScreen();
}

void HandleLock() {
  base::RecordAction(UserMetricsAction("Accel_LockScreen_L"));
  WmShell::Get()->GetSessionStateDelegate()->LockScreen();
}

void HandleShowStylusTools() {
  base::RecordAction(UserMetricsAction("Accel_Show_Stylus_Tools"));

  RootWindowController* root_window_controller =
      WmShell::Get()->GetRootWindowForNewWindows()->GetRootWindowController();
  PaletteTray* palette_tray =
      root_window_controller->GetShelf()->GetStatusAreaWidget()->palette_tray();
  palette_tray->ShowPalette();
}

bool CanHandleShowStylusTools() {
  return WmShell::Get()->palette_delegate() &&
         WmShell::Get()->palette_delegate()->ShouldShowPalette();
}

void HandleSuspend() {
  base::RecordAction(UserMetricsAction("Accel_Suspend"));
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestSuspend();
}

bool CanHandleCycleUser() {
  return WmShell::Get()->delegate()->IsMultiProfilesEnabled() &&
         WmShell::Get()->GetSessionStateDelegate()->NumberOfLoggedInUsers() > 1;
}

void HandleCycleUser(CycleUserDirection direction) {
  MultiProfileUMA::RecordSwitchActiveUser(
      MultiProfileUMA::SWITCH_ACTIVE_USER_BY_ACCELERATOR);
  switch (direction) {
    case CycleUserDirection::NEXT:
      base::RecordAction(UserMetricsAction("Accel_Switch_To_Next_User"));
      break;
    case CycleUserDirection::PREVIOUS:
      base::RecordAction(UserMetricsAction("Accel_Switch_To_Previous_User"));
      break;
  }
  WmShell::Get()->GetSessionStateDelegate()->CycleActiveUser(direction);
}

bool CanHandleToggleCapsLock(const ui::Accelerator& accelerator,
                             const ui::Accelerator& previous_accelerator) {
  chromeos::input_method::InputMethodManager* ime =
      chromeos::input_method::InputMethodManager::Get();

  // This shortcust is set to be trigger on release. Either the current
  // accelerator is a Search release or Alt release.
  if (accelerator.key_code() == ui::VKEY_LWIN &&
      accelerator.type() == ui::ET_KEY_RELEASED) {
    // The previous must be either an Alt press or Search press:
    // 1. Press Alt, Press Search, Release Search, Release Alt.
    // 2. Press Search, Press Alt, Release Search, Release Alt.
    if (previous_accelerator.type() == ui::ET_KEY_PRESSED &&
        (previous_accelerator.key_code() == ui::VKEY_LWIN ||
         previous_accelerator.key_code() == ui::VKEY_MENU)) {
      return ime && ime->GetImeKeyboard();
    }
  }

  // Alt release.
  if (accelerator.key_code() == ui::VKEY_MENU &&
      accelerator.type() == ui::ET_KEY_RELEASED) {
    // The previous must be either an Alt press or Search press:
    // 3. Press Alt, Press Search, Release Alt, Release Search.
    // 4. Press Search, Press Alt, Release Alt, Release Search.
    if (previous_accelerator.type() == ui::ET_KEY_PRESSED &&
        (previous_accelerator.key_code() == ui::VKEY_LWIN ||
         previous_accelerator.key_code() == ui::VKEY_MENU)) {
      return ime && ime->GetImeKeyboard();
    }
  }

  return false;
}

void HandleToggleCapsLock() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Caps_Lock"));
  chromeos::input_method::InputMethodManager* ime =
      chromeos::input_method::InputMethodManager::Get();
  chromeos::input_method::ImeKeyboard* keyboard = ime->GetImeKeyboard();
  keyboard->SetCapsLockEnabled(!keyboard->CapsLockIsEnabled());
}

void HandleToggleHighContrast() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_High_Contrast"));

  // Show a notification so the user knows that this accelerator toggled
  // high contrast mode, and that they can press it again to toggle back.
  // The message center automatically only shows this once per session.
  std::unique_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kHighContrastToggleAccelNotificationId, base::string16() /* title */,
      l10n_util::GetStringUTF16(IDS_HIGH_CONTRAST_ACCEL_MSG),
      gfx::Image(CreateVectorIcon(kSystemMenuAccessibilityIcon, SK_ColorBLACK)),
      base::string16() /* display source */, GURL(),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 system_notifier::kNotifierAccessibility),
      message_center::RichNotificationData(), nullptr));
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));

  WmShell::Get()->accessibility_delegate()->ToggleHighContrast();
}

void HandleToggleSpokenFeedback() {
  base::RecordAction(UserMetricsAction("Accel_Toggle_Spoken_Feedback"));

  WmShell::Get()->accessibility_delegate()->ToggleSpokenFeedback(
      A11Y_NOTIFICATION_SHOW);
}

void HandleVolumeDown(mojom::VolumeController* volume_controller,
                      const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_VOLUME_DOWN)
    base::RecordAction(UserMetricsAction("Accel_VolumeDown_F9"));

  if (volume_controller)
    volume_controller->VolumeDown();
}

void HandleVolumeMute(mojom::VolumeController* volume_controller,
                      const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_VOLUME_MUTE)
    base::RecordAction(UserMetricsAction("Accel_VolumeMute_F8"));

  if (volume_controller)
    volume_controller->VolumeMute();
}

void HandleVolumeUp(mojom::VolumeController* volume_controller,
                    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_VOLUME_UP)
    base::RecordAction(UserMetricsAction("Accel_VolumeUp_F10"));

  if (volume_controller)
    volume_controller->VolumeUp();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AcceleratorController, public:

AcceleratorController::AcceleratorController(
    AcceleratorControllerDelegate* delegate,
    ui::AcceleratorManagerDelegate* manager_delegate)
    : delegate_(delegate),
      accelerator_manager_(new ui::AcceleratorManager(manager_delegate)),
      accelerator_history_(new ui::AcceleratorHistory) {
  Init();
}

AcceleratorController::~AcceleratorController() {}

void AcceleratorController::Register(
    const std::vector<ui::Accelerator>& accelerators,
    ui::AcceleratorTarget* target) {
  accelerator_manager_->Register(
      accelerators, ui::AcceleratorManager::kNormalPriority, target);
}

void AcceleratorController::Unregister(const ui::Accelerator& accelerator,
                                       ui::AcceleratorTarget* target) {
  accelerator_manager_->Unregister(accelerator, target);
}

void AcceleratorController::UnregisterAll(ui::AcceleratorTarget* target) {
  accelerator_manager_->UnregisterAll(target);
}

bool AcceleratorController::Process(const ui::Accelerator& accelerator) {
  return accelerator_manager_->Process(accelerator);
}

bool AcceleratorController::IsRegistered(
    const ui::Accelerator& accelerator) const {
  return accelerator_manager_->IsRegistered(accelerator);
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

void AcceleratorController::SetImeControlDelegate(
    std::unique_ptr<ImeControlDelegate> ime_control_delegate) {
  ime_control_delegate_ = std::move(ime_control_delegate);
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
  if (!CanPerformAction(action, accelerator))
    return false;

  // Handling the deprecated accelerators (if any) only if action can be
  // performed.
  if (MaybeDeprecatedAcceleratorPressed(action, accelerator) ==
      AcceleratorProcessingStatus::STOP) {
    return false;
  }

  PerformAction(action, accelerator);
  return ShouldActionConsumeKeyEvent(action);
}

bool AcceleratorController::CanHandleAccelerators() const {
  return true;
}

void AcceleratorController::BindRequest(
    mojom::AcceleratorControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void AcceleratorController::SetVolumeController(
    mojom::VolumeControllerPtr controller) {
  volume_controller_ = std::move(controller);
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
  for (size_t i = 0; i < kRepeatableActionsLength; ++i)
    repeatable_actions_.insert(kRepeatableActions[i]);
  for (size_t i = 0; i < kActionsAllowedInAppModeOrPinnedModeLength; ++i) {
    actions_allowed_in_app_mode_.insert(
        kActionsAllowedInAppModeOrPinnedMode[i]);
    actions_allowed_in_pinned_mode_.insert(
        kActionsAllowedInAppModeOrPinnedMode[i]);
  }
  for (size_t i = 0; i < kActionsAllowedInPinnedModeLength; ++i)
    actions_allowed_in_pinned_mode_.insert(kActionsAllowedInPinnedMode[i]);
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

  if (debug::DeveloperAcceleratorsEnabled()) {
    RegisterAccelerators(kDeveloperAcceleratorData,
                         kDeveloperAcceleratorDataLength);
    // Developer accelerators are also reserved.
    for (size_t i = 0; i < kDeveloperAcceleratorDataLength; ++i)
      reserved_actions_.insert(kDeveloperAcceleratorData[i].action);
  }
}

void AcceleratorController::RegisterAccelerators(
    const AcceleratorData accelerators[],
    size_t accelerators_length) {
  std::vector<ui::Accelerator> ui_accelerators;
  for (size_t i = 0; i < accelerators_length; ++i) {
    ui::Accelerator accelerator =
        CreateAccelerator(accelerators[i].keycode, accelerators[i].modifiers,
                          accelerators[i].trigger_on_press);
    ui_accelerators.push_back(accelerator);
    accelerators_.insert(std::make_pair(accelerator, accelerators[i].action));
  }
  Register(ui_accelerators, this);
}

void AcceleratorController::RegisterDeprecatedAccelerators() {
  for (size_t i = 0; i < kDeprecatedAcceleratorsDataLength; ++i) {
    const DeprecatedAcceleratorData* data = &kDeprecatedAcceleratorsData[i];
    actions_with_deprecations_[data->action] = data;
  }

  std::vector<ui::Accelerator> ui_accelerators;
  for (size_t i = 0; i < kDeprecatedAcceleratorsLength; ++i) {
    const AcceleratorData& accelerator_data = kDeprecatedAccelerators[i];
    const ui::Accelerator deprecated_accelerator =
        CreateAccelerator(accelerator_data.keycode, accelerator_data.modifiers,
                          accelerator_data.trigger_on_press);

    ui_accelerators.push_back(deprecated_accelerator);
    accelerators_[deprecated_accelerator] = accelerator_data.action;
    deprecated_accelerators_.insert(deprecated_accelerator);
  }
  Register(ui_accelerators, this);
}

bool AcceleratorController::CanPerformAction(
    AcceleratorAction action,
    const ui::Accelerator& accelerator) {
  if (accelerator.IsRepeat() && !repeatable_actions_.count(action))
    return false;

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
    case CYCLE_BACKWARD_MRU:
    case CYCLE_FORWARD_MRU:
      return CanHandleCycleMru(accelerator);
    case DEBUG_PRINT_LAYER_HIERARCHY:
    case DEBUG_PRINT_VIEW_HIERARCHY:
    case DEBUG_PRINT_WINDOW_HIERARCHY:
    case DEBUG_SHOW_TOAST:
    case DEBUG_TOGGLE_TOUCH_PAD:
    case DEBUG_TOGGLE_TOUCH_SCREEN:
    case DEBUG_TOGGLE_TOUCH_VIEW:
    case DEBUG_TOGGLE_WALLPAPER_MODE:
    case DEBUG_TRIGGER_CRASH:
      return debug::DebugAcceleratorsEnabled();
    case DISABLE_CAPS_LOCK:
      return CanHandleDisableCapsLock(previous_accelerator);
    case LOCK_SCREEN:
      return CanHandleLock();
    case NEW_INCOGNITO_WINDOW:
      return CanHandleNewIncognitoWindow();
    case NEXT_IME:
      return CanHandleNextIme(ime_control_delegate_.get());
    case PREVIOUS_IME:
      return CanHandlePreviousIme(ime_control_delegate_.get());
    case SHOW_MESSAGE_CENTER_BUBBLE:
      return CanHandleShowMessageCenterBubble();
    case SHOW_STYLUS_TOOLS:
      return CanHandleShowStylusTools();
    case SWITCH_IME:
      return CanHandleSwitchIme(ime_control_delegate_.get(), accelerator);
    case SWITCH_TO_PREVIOUS_USER:
    case SWITCH_TO_NEXT_USER:
      return CanHandleCycleUser();
    case TOGGLE_APP_LIST:
      return CanHandleToggleAppList(accelerator, previous_accelerator);
    case TOGGLE_CAPS_LOCK:
      return CanHandleToggleCapsLock(accelerator, previous_accelerator);
    case WINDOW_CYCLE_SNAP_DOCK_LEFT:
    case WINDOW_CYCLE_SNAP_DOCK_RIGHT:
      return CanHandleWindowSnapOrDock();
    case WINDOW_POSITION_CENTER:
      return CanHandlePositionCenter();

    // The following are always enabled.
    case BRIGHTNESS_DOWN:
    case BRIGHTNESS_UP:
    case EXIT:
    case FOCUS_NEXT_PANE:
    case FOCUS_PREVIOUS_PANE:
    case FOCUS_SHELF:
    case KEYBOARD_BRIGHTNESS_DOWN:
    case KEYBOARD_BRIGHTNESS_UP:
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
    case OPEN_CROSH:
    case OPEN_FEEDBACK_PAGE:
    case OPEN_FILE_MANAGER:
    case OPEN_GET_HELP:
    case PRINT_UI_HIERARCHIES:
    case RESTORE_TAB:
    case SHOW_IME_MENU_BUBBLE:
    case SHOW_KEYBOARD_OVERLAY:
    case SHOW_TASK_MANAGER:
    case SUSPEND:
    case TOGGLE_FULLSCREEN:
    case TOGGLE_HIGH_CONTRAST:
    case TOGGLE_MAXIMIZED:
    case TOGGLE_OVERVIEW:
    case TOGGLE_SPOKEN_FEEDBACK:
    case TOGGLE_WIFI:
    case VOLUME_DOWN:
    case VOLUME_MUTE:
    case VOLUME_UP:
    case WINDOW_MINIMIZE:
      return true;

    default:
      // Default switch is temporary until mash transition complete. Needed as
      // some actions don't yet work with mash.
      break;
  }
  return delegate_ && delegate_->HandlesAction(action) &&
         delegate_->CanPerformAction(action, accelerator, previous_accelerator);
}

void AcceleratorController::PerformAction(AcceleratorAction action,
                                          const ui::Accelerator& accelerator) {
  AcceleratorProcessingRestriction restriction =
      GetAcceleratorProcessingRestriction(action);
  if (restriction != RESTRICTION_NONE)
    return;

  // If your accelerator invokes more than one line of code, please either
  // implement it in your module's controller code or pull it into a HandleFoo()
  // function above.
  switch (action) {
    case BRIGHTNESS_DOWN: {
      BrightnessControlDelegate* delegate =
          WmShell::Get()->brightness_control_delegate();
      if (delegate)
        delegate->HandleBrightnessDown(accelerator);
      break;
    }
    case BRIGHTNESS_UP: {
      BrightnessControlDelegate* delegate =
          WmShell::Get()->brightness_control_delegate();
      if (delegate)
        delegate->HandleBrightnessUp(accelerator);
      break;
    }
    case CYCLE_BACKWARD_MRU:
      HandleCycleBackwardMRU(accelerator);
      break;
    case CYCLE_FORWARD_MRU:
      HandleCycleForwardMRU(accelerator);
      break;
    case DEBUG_PRINT_LAYER_HIERARCHY:
    case DEBUG_PRINT_VIEW_HIERARCHY:
    case DEBUG_PRINT_WINDOW_HIERARCHY:
    case DEBUG_SHOW_TOAST:
    case DEBUG_TOGGLE_TOUCH_PAD:
    case DEBUG_TOGGLE_TOUCH_SCREEN:
    case DEBUG_TOGGLE_TOUCH_VIEW:
    case DEBUG_TOGGLE_WALLPAPER_MODE:
    case DEBUG_TRIGGER_CRASH:
      debug::PerformDebugActionIfEnabled(action);
      break;
    case DISABLE_CAPS_LOCK:
      HandleDisableCapsLock();
      break;
    case EXIT:
      // UMA metrics are recorded in the handler.
      exit_warning_handler_.HandleAccelerator();
      break;
    case FOCUS_NEXT_PANE:
      HandleRotatePaneFocus(FocusCycler::FORWARD);
      break;
    case FOCUS_PREVIOUS_PANE:
      HandleRotatePaneFocus(FocusCycler::BACKWARD);
      break;
    case FOCUS_SHELF:
      HandleFocusShelf();
      break;
    case KEYBOARD_BRIGHTNESS_DOWN: {
      KeyboardBrightnessControlDelegate* delegate =
          WmShell::Get()->keyboard_brightness_control_delegate();
      if (delegate)
        delegate->HandleKeyboardBrightnessDown(accelerator);
      break;
    }
    case KEYBOARD_BRIGHTNESS_UP: {
      KeyboardBrightnessControlDelegate* delegate =
          WmShell::Get()->keyboard_brightness_control_delegate();
      if (delegate)
        delegate->HandleKeyboardBrightnessUp(accelerator);
      break;
    }
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
    case LOCK_SCREEN:
      HandleLock();
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
    case OPEN_CROSH:
      HandleCrosh();
      break;
    case OPEN_FEEDBACK_PAGE:
      HandleOpenFeedbackPage();
      break;
    case OPEN_FILE_MANAGER:
      HandleFileManager();
      break;
    case OPEN_GET_HELP:
      HandleGetHelp();
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
    case SHOW_IME_MENU_BUBBLE:
      HandleShowImeMenuBubble();
      break;
    case SHOW_KEYBOARD_OVERLAY:
      HandleShowKeyboardOverlay();
      break;
    case SHOW_MESSAGE_CENTER_BUBBLE:
      HandleShowMessageCenterBubble();
      break;
    case SHOW_STYLUS_TOOLS:
      HandleShowStylusTools();
      break;
    case SHOW_TASK_MANAGER:
      HandleShowTaskManager();
      break;
    case SUSPEND:
      HandleSuspend();
      break;
    case SWITCH_IME:
      HandleSwitchIme(ime_control_delegate_.get(), accelerator);
      break;
    case SWITCH_TO_NEXT_USER:
      HandleCycleUser(CycleUserDirection::NEXT);
      break;
    case SWITCH_TO_PREVIOUS_USER:
      HandleCycleUser(CycleUserDirection::PREVIOUS);
      break;
    case TOGGLE_APP_LIST:
      HandleToggleAppList(accelerator);
      break;
    case TOGGLE_CAPS_LOCK:
      HandleToggleCapsLock();
      break;
    case TOGGLE_FULLSCREEN:
      HandleToggleFullscreen(accelerator);
      break;
    case TOGGLE_HIGH_CONTRAST:
      HandleToggleHighContrast();
      break;
    case TOGGLE_MAXIMIZED:
      accelerators::ToggleMaximized();
      break;
    case TOGGLE_OVERVIEW:
      HandleToggleOverview();
      break;
    case TOGGLE_SPOKEN_FEEDBACK:
      HandleToggleSpokenFeedback();
      break;
    case TOGGLE_WIFI:
      WmShell::Get()->system_tray_notifier()->NotifyRequestToggleWifi();
      break;
    case VOLUME_DOWN:
      HandleVolumeDown(volume_controller_.get(), accelerator);
      break;
    case VOLUME_MUTE:
      HandleVolumeMute(volume_controller_.get(), accelerator);
      break;
    case VOLUME_UP:
      HandleVolumeUp(volume_controller_.get(), accelerator);
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
    default:
      // Temporary until mash transition complete. Needed as some actions
      // don't yet work with mash.
      DCHECK(delegate_ && delegate_->HandlesAction(action));
      delegate_->PerformAction(action, accelerator);
      break;
  }
}

bool AcceleratorController::ShouldActionConsumeKeyEvent(
    AcceleratorAction action) {
  // Adding new exceptions is *STRONGLY* discouraged.
  return true;
}

AcceleratorController::AcceleratorProcessingRestriction
AcceleratorController::GetAcceleratorProcessingRestriction(int action) {
  WmShell* wm_shell = WmShell::Get();
  if (wm_shell->IsPinned() &&
      actions_allowed_in_pinned_mode_.find(action) ==
          actions_allowed_in_pinned_mode_.end()) {
    return RESTRICTION_PREVENT_PROCESSING_AND_PROPAGATION;
  }
  // TODO(xiyuan): Replace with SessionController. http://crbug.com/648964
  if (!wm_shell->GetSessionStateDelegate()->IsActiveUserSessionStarted() &&
      actions_allowed_at_login_screen_.find(action) ==
          actions_allowed_at_login_screen_.end()) {
    return RESTRICTION_PREVENT_PROCESSING;
  }
  if (wm_shell->GetSessionStateDelegate()->IsScreenLocked() &&
      actions_allowed_at_lock_screen_.find(action) ==
          actions_allowed_at_lock_screen_.end()) {
    return RESTRICTION_PREVENT_PROCESSING;
  }
  if (wm_shell->delegate()->IsRunningInForcedAppMode() &&
      actions_allowed_in_app_mode_.find(action) ==
          actions_allowed_in_app_mode_.end()) {
    return RESTRICTION_PREVENT_PROCESSING;
  }
  if (WmShell::Get()->IsSystemModalWindowOpen() &&
      actions_allowed_at_modal_window_.find(action) ==
          actions_allowed_at_modal_window_.end()) {
    // Note we prevent the shortcut from propagating so it will not
    // be passed to the modal window. This is important for things like
    // Alt+Tab that would cause an undesired effect in the modal window by
    // cycling through its window elements.
    return RESTRICTION_PREVENT_PROCESSING_AND_PROPAGATION;
  }
  if (wm_shell->mru_window_tracker()->BuildMruWindowList().empty() &&
      actions_needing_window_.find(action) != actions_needing_window_.end()) {
    wm_shell->accessibility_delegate()->TriggerAccessibilityAlert(
        A11Y_ALERT_WINDOW_NEEDED);
    return RESTRICTION_PREVENT_PROCESSING_AND_PROPAGATION;
  }
  return RESTRICTION_NONE;
}

AcceleratorController::AcceleratorProcessingStatus
AcceleratorController::MaybeDeprecatedAcceleratorPressed(
    AcceleratorAction action,
    const ui::Accelerator& accelerator) const {
  auto itr = actions_with_deprecations_.find(action);
  if (itr == actions_with_deprecations_.end()) {
    // The action is not associated with any deprecated accelerators, and hence
    // should be performed normally.
    return AcceleratorProcessingStatus::PROCEED;
  }

  // This action is associated with new and deprecated accelerators, find which
  // one is |accelerator|.
  const DeprecatedAcceleratorData* data = itr->second;
  if (!deprecated_accelerators_.count(accelerator)) {
    // This is a new accelerator replacing the old deprecated one.
    // Record UMA stats and proceed normally to perform it.
    RecordUmaHistogram(data->uma_histogram_name, NEW_USED);
    return AcceleratorProcessingStatus::PROCEED;
  }

  // This accelerator has been deprecated and should be treated according
  // to its |DeprecatedAcceleratorData|.

  // Record UMA stats.
  RecordUmaHistogram(data->uma_histogram_name, DEPRECATED_USED);

  if (delegate_) {
    // We always display the notification as long as this |data| entry exists.
    delegate_->ShowDeprecatedAcceleratorNotification(
        data->uma_histogram_name, data->notification_message_id,
        data->old_shortcut_id, data->new_shortcut_id);
  }

  if (!data->deprecated_enabled)
    return AcceleratorProcessingStatus::STOP;

  return AcceleratorProcessingStatus::PROCEED;
}

}  // namespace ash
