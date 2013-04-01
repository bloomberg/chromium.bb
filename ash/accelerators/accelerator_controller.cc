// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include "ash/accelerators/accelerator_table.h"
#include "ash/ash_switches.h"
#include "ash/caps_lock_delegate.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/user_wallpaper_delegate.h"
#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/focus_cycler.h"
#include "ash/ime_control_delegate.h"
#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_delegate.h"
#include "ash/launcher/launcher_model.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/magnifier/partial_magnification_controller.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation.h"
#include "ash/screenshot_delegate.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/system/brightness/brightness_control_delegate.h"
#include "ash/system/keyboard_brightness/keyboard_brightness_control_delegate.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/web_notification/web_notification_tray.h"
#include "ash/touch/touch_observer_hud.h"
#include "ash/volume_control_delegate.h"
#include "ash/wm/partial_screenshot_view.h"
#include "ash/wm/power_button_controller.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "content/public/browser/gpu_data_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/base/events/event.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/compositor/debug_utils.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/screen.h"
#include "ui/oak/oak.h"
#include "ui/views/debug_utils.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "ash/system/chromeos/keyboard_brightness_controller.h"
#include "base/chromeos/chromeos_version.h"
#endif  // defined(OS_CHROMEOS)

namespace ash {
namespace {

using internal::DisplayInfo;

// Factor of magnification scale. For example, when this value is 1.189, scale
// value will be changed x1.000, x1.189, x1.414, x1.681, x2.000, ...
// Note: this value is 2.0 ^ (1 / 4).
const float kMagnificationFactor = 1.18920712f;

bool DebugShortcutsEnabled() {
#if defined(NDEBUG)
  return CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshDebugShortcuts);
#else
  return true;
#endif
}

bool HandleCycleWindowMRU(WindowCycleController::Direction direction,
                          bool is_alt_down) {
  Shell::GetInstance()->
      window_cycle_controller()->HandleCycleWindow(direction, is_alt_down);
  // Always report we handled the key, even if the window didn't change.
  return true;
}

void HandleCycleWindowLinear(CycleDirection direction) {
  Shell::GetInstance()->
        window_cycle_controller()->HandleLinearCycleWindow();
}

#if defined(OS_CHROMEOS)
bool HandleLock() {
  Shell::GetInstance()->delegate()->LockScreen();
  return true;
}

bool HandleFileManager(bool as_dialog) {
  Shell::GetInstance()->delegate()->OpenFileManager(as_dialog);
  return true;
}

bool HandleCrosh() {
  Shell::GetInstance()->delegate()->OpenCrosh();
  return true;
}

bool HandleToggleSpokenFeedback() {
  Shell::GetInstance()->delegate()->
      ToggleSpokenFeedback(A11Y_NOTIFICATION_SHOW);
  return true;
}

#endif  // defined(OS_CHROMEOS)

bool HandleRotatePaneFocus(Shell::Direction direction) {
  if (!Shell::GetInstance()->delegate()->RotatePaneFocus(direction)) {
    // No browser window is available. Focus the launcher.
    Shell* shell = Shell::GetInstance();
    switch (direction) {
      case Shell::FORWARD:
        shell->focus_cycler()->RotateFocus(internal::FocusCycler::FORWARD);
        break;
      case Shell::BACKWARD:
        shell->focus_cycler()->RotateFocus(internal::FocusCycler::BACKWARD);
        break;
    }
  }
  return true;
}

// Rotate the active window.
bool HandleRotateActiveWindow() {
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

bool HandleScaleUI(bool up) {
  internal::DisplayManager* display_manager =
      Shell::GetInstance()->display_manager();
  int64 display_id = display_manager->GetDisplayIdForUIScaling();
  if (display_id == gfx::Display::kInvalidDisplayID)
    return false;
  const DisplayInfo& display_info = display_manager->GetDisplayInfo(display_id);
  float next_scale =
      internal::DisplayManager::GetNextUIScale(display_info.ui_scale(), up);
  display_manager->SetDisplayUIScale(display_id, next_scale);
  return true;
}

bool HandleScaleReset() {
  internal::DisplayManager* display_manager =
      Shell::GetInstance()->display_manager();
  int64 display_id = display_manager->GetDisplayIdForUIScaling();
  if (display_id == gfx::Display::kInvalidDisplayID)
    return false;
  display_manager->SetDisplayUIScale(display_id, 1.0f);
  return true;
}

// Rotates the screen.
bool HandleRotateScreen() {
  gfx::Point point = Shell::GetScreen()->GetCursorScreenPoint();
  gfx::Display display = Shell::GetScreen()->GetDisplayNearestPoint(point);
  const DisplayInfo& display_info =
      Shell::GetInstance()->display_manager()->GetDisplayInfo(display.id());
  Shell::GetInstance()->display_manager()->SetDisplayRotation(
      display.id(), GetNextRotation(display_info.rotation()));
  return true;
}

bool HandleToggleDesktopBackgroundMode() {
  DesktopBackgroundController* desktop_background_controller =
      Shell::GetInstance()->desktop_background_controller();
  if (desktop_background_controller->desktop_background_mode() ==
      DesktopBackgroundController::BACKGROUND_IMAGE) {
    desktop_background_controller->SetDesktopBackgroundSolidColorMode(
        SK_ColorBLACK);
  } else {
    ash::Shell::GetInstance()->user_wallpaper_delegate()->
        InitializeWallpaper();
  }
  return true;
}

bool HandleToggleRootWindowFullScreen() {
  Shell::GetPrimaryRootWindow()->ToggleFullScreen();
  return true;
}

// Magnify the screen
bool HandleMagnifyScreen(int delta_index) {
  if (ash::Shell::GetInstance()->magnification_controller()->IsEnabled()) {
    // TODO(yoshiki): Create the class like MagnifierStepScaleController, and
    // move the following scale control to it.
    float scale =
        ash::Shell::GetInstance()->magnification_controller()->GetScale();
    // Calculate rounded logarithm (base kMagnificationFactor) of scale.
    int scale_index =
        std::floor(std::log(scale) / std::log(kMagnificationFactor) + 0.5);

    int new_scale_index = std::max(0, std::min(8, scale_index + delta_index));

    ash::Shell::GetInstance()->magnification_controller()->
        SetScale(std::pow(kMagnificationFactor, new_scale_index), true);
  } else if (ash::Shell::GetInstance()->
             partial_magnification_controller()->is_enabled()) {
    float scale = delta_index > 0 ? kDefaultPartialMagnifiedScale : 1;
    ash::Shell::GetInstance()->partial_magnification_controller()->
        SetScale(scale);
  }

  return true;
}

bool HandleMediaNextTrack() {
  Shell::GetInstance()->delegate()->HandleMediaNextTrack();
  return true;
}

bool HandleMediaPlayPause() {
  Shell::GetInstance()->delegate()->HandleMediaPlayPause();
  return true;
}

bool HandleMediaPrevTrack() {
  Shell::GetInstance()->delegate()->HandleMediaPrevTrack();
  return true;
}

bool HandlePrintLayerHierarchy() {
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  for (size_t i = 0; i < root_windows.size(); ++i) {
    ui::PrintLayerHierarchy(root_windows[i]->layer(),
                            root_windows[i]->GetLastMouseLocationInRoot());
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
    PrintWindowHierarchy(controllers[i]->root_window(), 0, &out);
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

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AcceleratorControllerContext, public:

AcceleratorControllerContext::AcceleratorControllerContext() {
  current_accelerator_.set_type(ui::ET_UNKNOWN);
  previous_accelerator_.set_type(ui::ET_UNKNOWN);
}

void AcceleratorControllerContext::UpdateContext(
    const ui::Accelerator& accelerator) {
  previous_accelerator_ = current_accelerator_;
  current_accelerator_ = accelerator;
}

////////////////////////////////////////////////////////////////////////////////
// AcceleratorController, public:

AcceleratorController::AcceleratorController()
    : accelerator_manager_(new ui::AcceleratorManager) {
  Init();
}

AcceleratorController::~AcceleratorController() {
}

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
  for (size_t i = 0; i < kReservedActionsLength; ++i)
    reserved_actions_.insert(kReservedActions[i]);
  for (size_t i = 0; i < kNonrepeatableActionsLength; ++i)
    nonrepeatable_actions_.insert(kNonrepeatableActions[i]);
  for (size_t i = 0; i < kActionsAllowedInAppModeLength; ++i)
    actions_allowed_in_app_mode_.insert(kActionsAllowedInAppMode[i]);

  RegisterAccelerators(kAcceleratorData, kAcceleratorDataLength);

  if (DebugShortcutsEnabled())
    RegisterAccelerators(kDebugAcceleratorData, kDebugAcceleratorDataLength);

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
  if (ime_control_delegate_.get()) {
    return accelerator_manager_->Process(
        ime_control_delegate_->RemapAccelerator(accelerator));
  }
  return accelerator_manager_->Process(accelerator);
}

bool AcceleratorController::IsRegistered(
    const ui::Accelerator& accelerator) const {
  return accelerator_manager_->GetCurrentTarget(accelerator) != NULL;
}

bool AcceleratorController::IsReservedAccelerator(
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
  bool at_login_screen = false;
#if defined(OS_CHROMEOS)
  at_login_screen = !shell->delegate()->IsSessionStarted();
#endif
  if (at_login_screen &&
      actions_allowed_at_login_screen_.find(action) ==
      actions_allowed_at_login_screen_.end()) {
    return false;
  }
  if (shell->IsScreenLocked() &&
      actions_allowed_at_lock_screen_.find(action) ==
      actions_allowed_at_lock_screen_.end()) {
    return false;
  }
  if (shell->IsSystemModalWindowOpen() &&
      actions_allowed_at_modal_window_.find(action) ==
      actions_allowed_at_modal_window_.end()) {
    // Note: we return true. This indicates the shortcut is handled
    // and will not be passed to the modal window. This is important
    // for things like Alt+Tab that would cause an undesired effect
    // in the modal window by cycling through its window elements.
    return true;
  }
  if (shell->delegate()->IsRunningInForcedAppMode() &&
      actions_allowed_in_app_mode_.find(action) ==
      actions_allowed_in_app_mode_.end()) {
    return false;
  }

  const ui::KeyboardCode key_code = accelerator.key_code();
  // PerformAction() is performed from gesture controllers and passes
  // empty Accelerator() instance as the second argument. Such events
  // should never be suspended.
  const bool gesture_event = key_code == ui::VKEY_UNKNOWN;

  // Ignore accelerators invoked as repeated (while holding a key for a long
  // time, if their handling is nonrepeatable.
  if (nonrepeatable_actions_.find(action) != nonrepeatable_actions_.end() &&
      context_.repeated() && !gesture_event) {
    return true;
  }
  // Type of the previous accelerator. Used by NEXT_IME and DISABLE_CAPS_LOCK.
  const ui::EventType previous_event_type =
    context_.previous_accelerator().type();
  const ui::KeyboardCode previous_key_code =
    context_.previous_accelerator().key_code();

  // You *MUST* return true when some action is performed. Otherwise, this
  // function might be called *twice*, via BrowserView::PreHandleKeyboardEvent
  // and BrowserView::HandleKeyboardEvent, for a single accelerator press.
  switch (action) {
    case CYCLE_BACKWARD_MRU:
      if (key_code == ui::VKEY_TAB)
        shell->delegate()->RecordUserMetricsAction(UMA_ACCEL_PREVWINDOW_TAB);
      return HandleCycleWindowMRU(WindowCycleController::BACKWARD,
                                  accelerator.IsAltDown());
    case CYCLE_FORWARD_MRU:
      if (key_code == ui::VKEY_TAB)
        shell->delegate()->RecordUserMetricsAction(UMA_ACCEL_NEXTWINDOW_TAB);
      return HandleCycleWindowMRU(WindowCycleController::FORWARD,
                                  accelerator.IsAltDown());
    case CYCLE_BACKWARD_LINEAR:
      if (key_code == ui::VKEY_MEDIA_LAUNCH_APP1)
        shell->delegate()->RecordUserMetricsAction(UMA_ACCEL_PREVWINDOW_F5);
      HandleCycleWindowLinear(CYCLE_BACKWARD);
      return true;
    case CYCLE_FORWARD_LINEAR:
      if (key_code == ui::VKEY_MEDIA_LAUNCH_APP1)
        shell->delegate()->RecordUserMetricsAction(UMA_ACCEL_NEXTWINDOW_F5);
      HandleCycleWindowLinear(CYCLE_FORWARD);
      return true;
#if defined(OS_CHROMEOS)
    case CYCLE_DISPLAY_MODE:
      Shell::GetInstance()->display_controller()->CycleDisplayMode();
      return true;
    case LOCK_SCREEN:
      if (key_code == ui::VKEY_L)
        shell->delegate()->RecordUserMetricsAction(UMA_ACCEL_LOCK_SCREEN_L);
      return HandleLock();
    case OPEN_FILE_DIALOG:
      return HandleFileManager(true /* as_dialog */);
    case OPEN_FILE_MANAGER:
      return HandleFileManager(false /* as_dialog */);
    case OPEN_CROSH:
      return HandleCrosh();
    case SWAP_PRIMARY_DISPLAY:
      Shell::GetInstance()->display_controller()->SwapPrimaryDisplay();
      return true;
    case TOGGLE_SPOKEN_FEEDBACK:
      return HandleToggleSpokenFeedback();
    case TOGGLE_WIFI:
      Shell::GetInstance()->system_tray_delegate()->ToggleWifi();
      return true;
    case TOUCH_HUD_CLEAR: {
      internal::RootWindowController* controller =
          internal::RootWindowController::ForActiveRootWindow();
      if (controller->touch_observer_hud()) {
        controller->touch_observer_hud()->Clear();
        return true;
      }
      return false;
    }
    case TOUCH_HUD_MODE_CHANGE: {
      internal::RootWindowController* controller =
          internal::RootWindowController::ForActiveRootWindow();
      if (controller->touch_observer_hud()) {
        controller->touch_observer_hud()->ChangeToNextMode();
        return true;
      }
      return false;
    }
    case DISABLE_GPU_WATCHDOG:
      content::GpuDataManager::GetInstance()->DisableGpuWatchdog();
      return true;
#endif
    case OPEN_FEEDBACK_PAGE:
      ash::Shell::GetInstance()->delegate()->OpenFeedbackPage();
      return true;
    case EXIT:
      Shell::GetInstance()->delegate()->Exit();
      return true;
    case NEW_INCOGNITO_WINDOW:
      Shell::GetInstance()->delegate()->NewWindow(true /* is_incognito */);
      return true;
    case NEW_TAB:
      if (key_code == ui::VKEY_T)
        shell->delegate()->RecordUserMetricsAction(UMA_ACCEL_NEWTAB_T);
      Shell::GetInstance()->delegate()->NewTab();
      return true;
    case NEW_WINDOW:
      Shell::GetInstance()->delegate()->NewWindow(false /* is_incognito */);
      return true;
    case RESTORE_TAB:
      Shell::GetInstance()->delegate()->RestoreTab();
      return true;
    case TAKE_SCREENSHOT:
      if (screenshot_delegate_.get() &&
          screenshot_delegate_->CanTakeScreenshot()) {
        screenshot_delegate_->HandleTakeScreenshotForAllRootWindows();
      }
      // Return true to prevent propagation of the key event.
      return true;
    case TAKE_PARTIAL_SCREENSHOT:
      if (screenshot_delegate_.get()) {
        ash::PartialScreenshotView::StartPartialScreenshot(
            screenshot_delegate_.get());
      }
      // Return true to prevent propagation of the key event because
      // this key combination is reserved for partial screenshot.
      return true;
    case TOGGLE_APP_LIST:
      // If something else was pressed between the Search key (LWIN)
      // being pressed and released, then ignore the release of the
      // Search key.
      if (key_code == ui::VKEY_LWIN &&
          (previous_event_type == ui::ET_KEY_RELEASED ||
           previous_key_code != ui::VKEY_LWIN))
        return false;
      if (key_code == ui::VKEY_LWIN)
        shell->delegate()->RecordUserMetricsAction(UMA_ACCEL_SEARCH_LWIN);
      // When spoken feedback is enabled, we should neither toggle the list nor
      // consume the key since Search+Shift is one of the shortcuts the a11y
      // feature uses. crbug.com/132296
      DCHECK_EQ(ui::VKEY_LWIN, accelerator.key_code());
      if (Shell::GetInstance()->delegate()->IsSpokenFeedbackEnabled())
        return false;
      ash::Shell::GetInstance()->ToggleAppList(NULL);
      return true;
    case DISABLE_CAPS_LOCK:
      if (previous_event_type == ui::ET_KEY_RELEASED ||
          (previous_key_code != ui::VKEY_LSHIFT &&
           previous_key_code != ui::VKEY_SHIFT &&
           previous_key_code != ui::VKEY_RSHIFT)) {
        // If something else was pressed between the Shift key being pressed
        // and released, then ignore the release of the Shift key.
        return false;
      }
      if (shell->caps_lock_delegate()->IsCapsLockEnabled()) {
        shell->caps_lock_delegate()->SetCapsLockEnabled(false);
        return true;
      }
      return false;
    case TOGGLE_CAPS_LOCK:
      if (key_code == ui::VKEY_LWIN) {
        // If something else was pressed between the Search key (LWIN)
        // being pressed and released, then ignore the release of the
        // Search key.
        // TODO(danakj): Releasing Alt first breaks this: crbug.com/166495
        if (previous_event_type == ui::ET_KEY_RELEASED ||
            previous_key_code != ui::VKEY_LWIN)
          return false;
      }
      shell->caps_lock_delegate()->ToggleCapsLock();
      return true;
    case BRIGHTNESS_DOWN:
      if (brightness_control_delegate_.get())
        return brightness_control_delegate_->HandleBrightnessDown(accelerator);
      break;
    case BRIGHTNESS_UP:
      if (brightness_control_delegate_.get())
        return brightness_control_delegate_->HandleBrightnessUp(accelerator);
      break;
    case KEYBOARD_BRIGHTNESS_DOWN:
      if (keyboard_brightness_control_delegate_.get())
        return keyboard_brightness_control_delegate_->
            HandleKeyboardBrightnessDown(accelerator);
      break;
    case KEYBOARD_BRIGHTNESS_UP:
      if (keyboard_brightness_control_delegate_.get())
        return keyboard_brightness_control_delegate_->
            HandleKeyboardBrightnessUp(accelerator);
      break;
    case VOLUME_MUTE:
      return shell->system_tray_delegate()->GetVolumeControlDelegate()->
          HandleVolumeMute(accelerator);
      break;
    case VOLUME_DOWN:
      return shell->system_tray_delegate()->GetVolumeControlDelegate()->
          HandleVolumeDown(accelerator);
      break;
    case VOLUME_UP:
      return shell->system_tray_delegate()->GetVolumeControlDelegate()->
          HandleVolumeUp(accelerator);
      break;
    case FOCUS_LAUNCHER:
      return shell->focus_cycler()->FocusWidget(
          Launcher::ForPrimaryDisplay()->shelf_widget());
      break;
    case FOCUS_NEXT_PANE:
      return HandleRotatePaneFocus(Shell::FORWARD);
    case FOCUS_PREVIOUS_PANE:
      return HandleRotatePaneFocus(Shell::BACKWARD);
    case SHOW_KEYBOARD_OVERLAY:
      ash::Shell::GetInstance()->delegate()->ShowKeyboardOverlay();
      return true;
    case SHOW_OAK:
      if (CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAshEnableOak)) {
        oak::ShowOakWindowWithContext(Shell::GetPrimaryRootWindow());
        return true;
      }
      break;
    case SHOW_SYSTEM_TRAY_BUBBLE: {
      internal::RootWindowController* controller =
          Shell::IsLauncherPerDisplayEnabled() ?
          internal::RootWindowController::ForActiveRootWindow() :
          Shell::GetPrimaryRootWindowController();
      if (!controller->GetSystemTray()->HasSystemBubble())
        controller->GetSystemTray()->ShowDefaultView(BUBBLE_CREATE_NEW);
      break;
    }
    case SHOW_MESSAGE_CENTER_BUBBLE: {
      internal::RootWindowController* controller =
          Shell::IsLauncherPerDisplayEnabled() ?
          internal::RootWindowController::ForActiveRootWindow() :
          Shell::GetPrimaryRootWindowController();
      internal::StatusAreaWidget* status_area_widget =
          controller->shelf()->status_area_widget();
      if (status_area_widget) {
        WebNotificationTray* notification_tray =
            status_area_widget->web_notification_tray();
        if (notification_tray->visible())
          notification_tray->ShowMessageCenterBubble();
      }
      break;
    }
    case SHOW_TASK_MANAGER:
      Shell::GetInstance()->delegate()->ShowTaskManager();
      return true;
    case NEXT_IME:
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
        return false;
      }
      if (ime_control_delegate_.get())
        return ime_control_delegate_->HandleNextIme();
      break;
    case PREVIOUS_IME:
      if (ime_control_delegate_.get())
        return ime_control_delegate_->HandlePreviousIme();
      break;
    case PRINT_UI_HIERARCHIES:
      return HandlePrintUIHierarchies();
    case SWITCH_IME:
      if (ime_control_delegate_.get())
        return ime_control_delegate_->HandleSwitchIme(accelerator);
      break;
    case SELECT_WIN_0:
      Launcher::ForPrimaryDisplay()->SwitchToWindow(0);
      return true;
    case SELECT_WIN_1:
      Launcher::ForPrimaryDisplay()->SwitchToWindow(1);
      return true;
    case SELECT_WIN_2:
      Launcher::ForPrimaryDisplay()->SwitchToWindow(2);
      return true;
    case SELECT_WIN_3:
      Launcher::ForPrimaryDisplay()->SwitchToWindow(3);
      return true;
    case SELECT_WIN_4:
      Launcher::ForPrimaryDisplay()->SwitchToWindow(4);
      return true;
    case SELECT_WIN_5:
      Launcher::ForPrimaryDisplay()->SwitchToWindow(5);
      return true;
    case SELECT_WIN_6:
      Launcher::ForPrimaryDisplay()->SwitchToWindow(6);
      return true;
    case SELECT_WIN_7:
      Launcher::ForPrimaryDisplay()->SwitchToWindow(7);
      return true;
    case SELECT_LAST_WIN:
      Launcher::ForPrimaryDisplay()->SwitchToWindow(-1);
      return true;
    case WINDOW_SNAP_LEFT:
    case WINDOW_SNAP_RIGHT: {
      aura::Window* window = wm::GetActiveWindow();
      // Disable window docking shortcut key for full screen window due to
      // http://crbug.com/135487.
      if (!window ||
          window->type() != aura::client::WINDOW_TYPE_NORMAL ||
          wm::IsWindowFullscreen(window)) {
        break;
      }

      internal::SnapSizer::SnapWindow(window,
          action == WINDOW_SNAP_LEFT ? internal::SnapSizer::LEFT_EDGE :
                                       internal::SnapSizer::RIGHT_EDGE);
      return true;
    }
    case WINDOW_MINIMIZE: {
      aura::Window* window = wm::GetActiveWindow();
      // Attempt to restore the window that would be cycled through next from
      // the launcher when there is no active window.
      if (!window)
        return HandleCycleWindowMRU(WindowCycleController::FORWARD, false);
      // Disable the shortcut for minimizing full screen window due to
      // crbug.com/131709, which is a crashing issue related to minimizing
      // full screen pepper window.
      if (!wm::IsWindowFullscreen(window) && wm::CanMinimizeWindow(window)) {
        wm::MinimizeWindow(window);
        return true;
      }
      break;
    }
    case TOGGLE_MAXIMIZED: {
      if (key_code == ui::VKEY_MEDIA_LAUNCH_APP2) {
        shell->delegate()->RecordUserMetricsAction(
            UMA_ACCEL_MAXIMIZE_RESTORE_F4);
      }
      shell->delegate()->ToggleMaximized();
      return true;
    }
    case WINDOW_POSITION_CENTER: {
      aura::Window* window = wm::GetActiveWindow();
      if (window) {
        wm::CenterWindow(window);
        return true;
      }
      break;
    }
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
      return HandleToggleDesktopBackgroundMode();
    case TOGGLE_ROOT_WINDOW_FULL_SCREEN:
      return HandleToggleRootWindowFullScreen();
    case DISPLAY_TOGGLE_SCALE:
      internal::DisplayManager::ToggleDisplayScaleFactor();
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
      if (!base::chromeos::IsRunningOnChromeOS()) {
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
#if !defined(NDEBUG)
    case PRINT_LAYER_HIERARCHY:
      return HandlePrintLayerHierarchy();
    case PRINT_VIEW_HIERARCHY:
      return HandlePrintViewHierarchy();
    case PRINT_WINDOW_HIERARCHY:
      return HandlePrintWindowHierarchy();
#endif
    default:
      NOTREACHED() << "Unhandled action " << action;
  }
  return false;
}

void AcceleratorController::SetBrightnessControlDelegate(
    scoped_ptr<BrightnessControlDelegate> brightness_control_delegate) {
  // Install brightness control delegate only when internal
  // display exists.
  if (Shell::GetInstance()->display_manager()->HasInternalDisplay() ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshEnableBrightnessControl)) {
    brightness_control_delegate_ = brightness_control_delegate.Pass();
  }
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
