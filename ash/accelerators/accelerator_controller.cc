// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller.h"

#include "ash/caps_lock_delegate.h"
#include "ash/ime_control_delegate.h"
#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_model.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/system/brightness/brightness_control_delegate.h"
#include "ash/volume_control_delegate.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_util.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/gfx/compositor/debug_utils.h"
#include "ui/gfx/compositor/layer_animation_sequence.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/compositor/screen_rotation.h"

namespace {

enum AcceleratorAction {
  BRIGHTNESS_DOWN,
  BRIGHTNESS_UP,
  CYCLE_BACKWARD,
  CYCLE_FORWARD,
  EXIT,
  NEXT_IME,
  PREVIOUS_IME,
  SWITCH_IME,  // Switch to another IME depending on the accelerator.
  TAKE_SCREENSHOT,
  TAKE_PARTIAL_SCREENSHOT,
  TOGGLE_APP_LIST,
  TOGGLE_CAPS_LOCK,
  VOLUME_DOWN,
  VOLUME_MUTE,
  VOLUME_UP,
#if defined(OS_CHROMEOS)
  LOCK_SCREEN,
#endif
#if !defined(NDEBUG)
  PRINT_LAYER_HIERARCHY,
  PRINT_WINDOW_HIERARCHY,
  ROTATE_SCREEN,
  TOGGLE_DESKTOP_BACKGROUND_MODE,
  TOGGLE_ROOT_WINDOW_FULL_SCREEN,
#endif
};

// Accelerators handled by AcceleratorController.
const struct AcceleratorData {
  ui::EventType type;
  ui::KeyboardCode keycode;
  bool shift;
  bool ctrl;
  bool alt;
  AcceleratorAction action;
} kAcceleratorData[] = {
  // Accelerators that should be processed before a key is sent to an IME.
  { ui::ET_KEY_RELEASED, ui::VKEY_MENU, true, false, true, NEXT_IME },
  { ui::ET_KEY_RELEASED, ui::VKEY_SHIFT, true, false, true, NEXT_IME },
  { ui::ET_KEY_PRESSED, ui::VKEY_SPACE, false, true, false, PREVIOUS_IME },
  // Shortcuts for Japanese IME.
  { ui::ET_KEY_PRESSED, ui::VKEY_CONVERT, false, false, false, SWITCH_IME },
  { ui::ET_KEY_PRESSED, ui::VKEY_NONCONVERT, false, false, false, SWITCH_IME },
  { ui::ET_KEY_PRESSED, ui::VKEY_DBE_SBCSCHAR, false, false, false,
    SWITCH_IME },
  { ui::ET_KEY_PRESSED, ui::VKEY_DBE_DBCSCHAR, false, false, false,
    SWITCH_IME },
  // Shortcuts for Koren IME.
  { ui::ET_KEY_PRESSED, ui::VKEY_HANGUL, false, false, false, SWITCH_IME },
  { ui::ET_KEY_PRESSED, ui::VKEY_SPACE, true, false, false, SWITCH_IME },

  // Accelerators that should be processed after a key is sent to an IME.
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_TAB, false, false, true,
    CYCLE_FORWARD },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_TAB, true, false, true,
    CYCLE_BACKWARD },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F5, false, false, false,
    CYCLE_FORWARD },
#if defined(OS_CHROMEOS)
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_BRIGHTNESS_DOWN, false, false, false,
    BRIGHTNESS_DOWN },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_BRIGHTNESS_UP, false, false, false,
    BRIGHTNESS_UP },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_L, true, true, false, LOCK_SCREEN },
#endif
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_Q, true, true, false, EXIT },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F5, true, false, false,
    CYCLE_BACKWARD },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F5, false, true, false,
    TAKE_SCREENSHOT },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F5, true, true, false,
    TAKE_PARTIAL_SCREENSHOT },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_PRINT, false, false, false,
    TAKE_SCREENSHOT },
  // On Chrome OS, Search key is mapped to LWIN.
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_LWIN, false, true, false,
    TOGGLE_APP_LIST },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_LWIN, true, false, false,
    TOGGLE_CAPS_LOCK },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F6, false, false, false,
    BRIGHTNESS_DOWN },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F7, false, false, false,
    BRIGHTNESS_UP },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F8, false, false, false,
    VOLUME_MUTE },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_VOLUME_MUTE, false, false, false,
    VOLUME_MUTE },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F9, false, false, false,
    VOLUME_DOWN },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_VOLUME_DOWN, false, false, false,
    VOLUME_DOWN },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F10, false, false, false, VOLUME_UP },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_VOLUME_UP, false, false, false,
    VOLUME_UP },
#if !defined(NDEBUG)
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_HOME, false, true, false,
    ROTATE_SCREEN },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_B, false, true, true,
    TOGGLE_DESKTOP_BACKGROUND_MODE },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_F11, false, true, false,
    TOGGLE_ROOT_WINDOW_FULL_SCREEN },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_L, false, false, true,
    PRINT_LAYER_HIERARCHY },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_L, true, false, true,
    PRINT_WINDOW_HIERARCHY },
  // For testing on systems where Alt-Tab is already mapped.
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_W, false, false, true,
    CYCLE_FORWARD },
  { ui::ET_TRANSLATED_KEY_PRESS, ui::VKEY_W, true, false, true,
    CYCLE_BACKWARD },
#endif
};

bool HandleCycleWindow(ash::WindowCycleController::Direction direction,
                       bool is_alt_down) {
  ash::Shell::GetInstance()->
      window_cycle_controller()->HandleCycleWindow(direction, is_alt_down);
  // Always report we handled the key, even if the window didn't change.
  return true;
}

#if defined(OS_CHROMEOS)
bool HandleLock() {
  ash::ShellDelegate* delegate = ash::Shell::GetInstance()->delegate();
  if (!delegate)
    return false;
  delegate->LockScreen();
  return true;
}
#endif

bool HandleExit() {
  ash::ShellDelegate* delegate = ash::Shell::GetInstance()->delegate();
  if (!delegate)
    return false;
  delegate->Exit();
  return true;
}

#if !defined(NDEBUG)
// Rotates the screen.
bool HandleRotateScreen() {
  static int i = 0;
  int delta = 0;
  switch (i) {
    case 0: delta = 90; break;
    case 1: delta = 90; break;
    case 2: delta = 90; break;
    case 3: delta = 90; break;
    case 4: delta = -90; break;
    case 5: delta = -90; break;
    case 6: delta = -90; break;
    case 7: delta = -90; break;
    case 8: delta = -90; break;
    case 9: delta = 180; break;
    case 10: delta = 180; break;
    case 11: delta = 90; break;
    case 12: delta = 180; break;
    case 13: delta = 180; break;
  }
  i = (i + 1) % 14;
  ash::Shell::GetRootWindow()->layer()->GetAnimator()->
      set_preemption_strategy(ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  scoped_ptr<ui::LayerAnimationSequence> screen_rotation(
      new ui::LayerAnimationSequence(new ui::ScreenRotation(delta)));
  screen_rotation->AddObserver(ash::Shell::GetRootWindow());
  ash::Shell::GetRootWindow()->layer()->GetAnimator()->StartAnimation(
      screen_rotation.release());
  return true;
}

bool HandleToggleDesktopBackgroundMode() {
  ash::Shell* shell = ash::Shell::GetInstance();
  if (shell->desktop_background_mode() == ash::Shell::BACKGROUND_IMAGE)
    shell->SetDesktopBackgroundMode(ash::Shell::BACKGROUND_SOLID_COLOR);
  else
    shell->SetDesktopBackgroundMode(ash::Shell::BACKGROUND_IMAGE);
  return true;
}

bool HandleToggleRootWindowFullScreen() {
  ash::Shell::GetRootWindow()->ToggleFullScreen();
  return true;
}

bool HandlePrintLayerHierarchy() {
  aura::RootWindow* root_window = ash::Shell::GetRootWindow();
  ui::PrintLayerHierarchy(root_window->layer(),
                          root_window->last_mouse_location());
  return true;
}

void PrintWindowHierarchy(aura::Window* window, int indent) {
  std::string indent_str(indent, ' ');
  VLOG(1) << indent_str << window->name() << " type " << window->type()
      << (ash::wm::IsActiveWindow(window) ? "active" : "");
  for (size_t i = 0; i < window->children().size(); ++i)
    PrintWindowHierarchy(window->children()[i], indent + 3);
}

bool HandlePrintWindowHierarchy() {
  VLOG(1) << "Window hierarchy:";
  aura::Window* container = ash::Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_DefaultContainer);
  PrintWindowHierarchy(container, 0);
  return true;
}

#endif

}  // namespace

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// AcceleratorController, public:

AcceleratorController::AcceleratorController()
    : accelerator_manager_(new ui::AcceleratorManager) {
  Init();
}

AcceleratorController::~AcceleratorController() {
}

void AcceleratorController::Init() {
  for (size_t i = 0; i < arraysize(kAcceleratorData); ++i) {
    ui::Accelerator accelerator(kAcceleratorData[i].keycode,
                                kAcceleratorData[i].shift,
                                kAcceleratorData[i].ctrl,
                                kAcceleratorData[i].alt);
    accelerator.set_type(kAcceleratorData[i].type);
    Register(accelerator, this);
    CHECK(accelerators_.insert(
        std::make_pair(accelerator, kAcceleratorData[i].action)).second);
  }
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
  return accelerator_manager_->Process(accelerator);
}

void AcceleratorController::SetBrightnessControlDelegate(
    scoped_ptr<BrightnessControlDelegate> brightness_control_delegate) {
  brightness_control_delegate_.swap(brightness_control_delegate);
}

void AcceleratorController::SetCapsLockDelegate(
    scoped_ptr<CapsLockDelegate> caps_lock_delegate) {
  caps_lock_delegate_.swap(caps_lock_delegate);
}

void AcceleratorController::SetImeControlDelegate(
    scoped_ptr<ImeControlDelegate> ime_control_delegate) {
  ime_control_delegate_.swap(ime_control_delegate);
}

void AcceleratorController::SetScreenshotDelegate(
    scoped_ptr<ScreenshotDelegate> screenshot_delegate) {
  screenshot_delegate_.swap(screenshot_delegate);
}

void AcceleratorController::SetVolumeControlDelegate(
    scoped_ptr<VolumeControlDelegate> volume_control_delegate) {
  volume_control_delegate_.swap(volume_control_delegate);
}

////////////////////////////////////////////////////////////////////////////////
// AcceleratorController, ui::AcceleratorTarget implementation:

bool AcceleratorController::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  std::map<ui::Accelerator, int>::const_iterator it =
      accelerators_.find(accelerator);
  DCHECK(it != accelerators_.end());
  switch (static_cast<AcceleratorAction>(it->second)) {
    case CYCLE_BACKWARD:
      return HandleCycleWindow(WindowCycleController::BACKWARD,
                               accelerator.IsAltDown());
    case CYCLE_FORWARD:
      return HandleCycleWindow(WindowCycleController::FORWARD,
                               accelerator.IsAltDown());
#if defined(OS_CHROMEOS)
    case LOCK_SCREEN:
      return HandleLock();
#endif
    case EXIT:
      return HandleExit();
    case TAKE_SCREENSHOT:
      if (screenshot_delegate_.get()) {
        aura::RootWindow* root_window = Shell::GetRootWindow();
        screenshot_delegate_->HandleTakeScreenshot(root_window);
      }
      // Return true to prevent propagation of the key event.
      return true;
    case TAKE_PARTIAL_SCREENSHOT:
      if (screenshot_delegate_.get())
        ash::Shell::GetInstance()->delegate()->
            StartPartialScreenshot(screenshot_delegate_.get());
      // Return true to prevent propagation of the key event because
      // this key combination is reserved for partial screenshot.
      return true;
    case TOGGLE_APP_LIST:
      ash::Shell::GetInstance()->ToggleAppList();
      break;
    case TOGGLE_CAPS_LOCK:
      if (caps_lock_delegate_.get())
        return caps_lock_delegate_->HandleToggleCapsLock();
      break;
    case BRIGHTNESS_DOWN:
      if (brightness_control_delegate_.get())
        return brightness_control_delegate_->HandleBrightnessDown(accelerator);
      break;
    case BRIGHTNESS_UP:
      if (brightness_control_delegate_.get())
        return brightness_control_delegate_->HandleBrightnessUp(accelerator);
      break;
    case VOLUME_MUTE:
      if (volume_control_delegate_.get())
        return volume_control_delegate_->HandleVolumeMute(accelerator);
      break;
    case VOLUME_DOWN:
      if (volume_control_delegate_.get())
        return volume_control_delegate_->HandleVolumeDown(accelerator);
      break;
    case VOLUME_UP:
      if (volume_control_delegate_.get())
        return volume_control_delegate_->HandleVolumeUp(accelerator);
      break;
    case NEXT_IME:
      if (ime_control_delegate_.get())
        return ime_control_delegate_->HandleNextIme();
      break;
    case PREVIOUS_IME:
      if (ime_control_delegate_.get())
        return ime_control_delegate_->HandlePreviousIme();
      break;
    case SWITCH_IME:
      if (ime_control_delegate_.get())
        return ime_control_delegate_->HandleSwitchIme(accelerator);
      break;
#if !defined(NDEBUG)
    case ROTATE_SCREEN:
      return HandleRotateScreen();
    case TOGGLE_DESKTOP_BACKGROUND_MODE:
      return HandleToggleDesktopBackgroundMode();
    case TOGGLE_ROOT_WINDOW_FULL_SCREEN:
      return HandleToggleRootWindowFullScreen();
    case PRINT_LAYER_HIERARCHY:
      return HandlePrintLayerHierarchy();
    case PRINT_WINDOW_HIERARCHY:
      return HandlePrintWindowHierarchy();
#endif
    default:
      NOTREACHED() << "Unhandled action " << it->second;
  }
  return false;
}

bool AcceleratorController::CanHandleAccelerators() const {
  return true;
}

}  // namespace ash
