// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_filter.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/events/event.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

const int kModifierFlagMask = (ui::EF_SHIFT_DOWN |
                               ui::EF_CONTROL_DOWN |
                               ui::EF_ALT_DOWN);

// Returns true if |key_code| is a key usually handled directly by the shell.
bool IsSystemKey(ui::KeyboardCode key_code) {
#if defined(OS_CHROMEOS)
  switch (key_code) {
    case ui::VKEY_MEDIA_LAUNCH_APP2:  // Fullscreen button.
    case ui::VKEY_MEDIA_LAUNCH_APP1:  // Overview button.
    case ui::VKEY_BRIGHTNESS_DOWN:
    case ui::VKEY_BRIGHTNESS_UP:
    case ui::VKEY_KBD_BRIGHTNESS_DOWN:
    case ui::VKEY_KBD_BRIGHTNESS_UP:
    case ui::VKEY_VOLUME_MUTE:
    case ui::VKEY_VOLUME_DOWN:
    case ui::VKEY_VOLUME_UP:
      return true;
    default:
      return false;
  }
#endif  // defined(OS_CHROMEOS)
  return false;
}

// Returns true if the window should be allowed a chance to handle system keys.
// Uses the top level window so if the target is a web contents window the
// containing parent window will be checked for the property.
bool CanConsumeSystemKeys(aura::Window* target) {
  if (!target)  // Can be NULL in tests.
    return false;
  aura::Window* top_level = ::wm::GetToplevelWindow(target);
  return top_level && wm::GetWindowState(top_level)->can_consume_system_keys();
}

// Returns true if the |accelerator| should be processed now, inside Ash's env
// event filter.
bool ShouldProcessAcceleratorsNow(const ui::Accelerator& accelerator,
                                  aura::Window* target) {
  if (!target)
    return true;

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  if (std::find(root_windows.begin(), root_windows.end(), target) !=
      root_windows.end())
    return true;

  // A full screen window should be able to handle all key events including the
  // reserved ones.
  if (wm::GetWindowState(target)->IsFullscreen()) {
    // TODO(yusukes): On Chrome OS, only browser and flash windows can be full
    // screen. Launching an app in "open full-screen" mode is not supported yet.
    // That makes the IsWindowFullscreen() check above almost meaningless
    // because a browser and flash window do handle Ash accelerators anyway
    // before they're passed to a page or flash content.
    return false;
  }

  if (Shell::GetInstance()->GetAppListTargetVisibility())
    return true;

  // Unless |target| is in the full screen state, handle reserved accelerators
  // such as Alt+Tab now.
  return Shell::GetInstance()->accelerator_controller()->IsReservedAccelerator(
      accelerator);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AcceleratorFilter, public:

AcceleratorFilter::AcceleratorFilter() {
}

AcceleratorFilter::~AcceleratorFilter() {
}

////////////////////////////////////////////////////////////////////////////////
// AcceleratorFilter, EventFilter implementation:

void AcceleratorFilter::OnKeyEvent(ui::KeyEvent* event) {
  const ui::EventType type = event->type();
  if (type != ui::ET_KEY_PRESSED && type != ui::ET_KEY_RELEASED)
    return;
  if (event->is_char())
    return;

  ui::Accelerator accelerator(event->key_code(),
                              event->flags() & kModifierFlagMask);
  accelerator.set_type(type);

  // Fill out context object so AcceleratorController will know what
  // was the previous accelerator or if the current accelerator is repeated.
  AcceleratorController* accelerator_controller =
      Shell::GetInstance()->accelerator_controller();
  accelerator_controller->context()->UpdateContext(accelerator);

  aura::Window* target = static_cast<aura::Window*>(event->target());
  // Handle special hardware keys like brightness and volume. However, some
  // windows can override this behavior (e.g. Chrome v1 apps by default and
  // Chrome v2 apps with permission) by setting a window property.
  if (IsSystemKey(event->key_code()) && !CanConsumeSystemKeys(target)) {
    accelerator_controller->Process(accelerator);
    // These keys are always consumed regardless of whether they trigger an
    // accelerator to prevent windows from seeing unexpected key up events.
    event->StopPropagation();
    return;
  }
  if (!ShouldProcessAcceleratorsNow(accelerator, target))
    return;
  if (accelerator_controller->Process(accelerator))
    event->StopPropagation();
}

}  // namespace ash
