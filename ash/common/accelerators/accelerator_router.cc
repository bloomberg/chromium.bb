// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/accelerators/accelerator_router.h"

#include "ash/common/accelerators/accelerator_controller.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"

namespace ash {

namespace {

// Returns true if |key_code| is a key usually handled directly by the shell.
bool IsSystemKey(ui::KeyboardCode key_code) {
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
    case ui::VKEY_POWER:
      return true;
    default:
      return false;
  }
}

}  // namespace

AcceleratorRouter::AcceleratorRouter() {}

AcceleratorRouter::~AcceleratorRouter() {}

bool AcceleratorRouter::ProcessAccelerator(WmWindow* target,
                                           const ui::KeyEvent& key_event,
                                           const ui::Accelerator& accelerator) {
  // Callers should never supply null.
  DCHECK(target);
  RecordSearchKeyStats(accelerator);
  // Special hardware keys like brightness and volume are handled in
  // special way. However, some windows can override this behavior
  // (e.g. Chrome v1 apps by default and Chrome v2 apps with
  // permission) by setting a window property.
  if (IsSystemKey(key_event.key_code()) &&
      !CanConsumeSystemKeys(target, key_event)) {
    // System keys are always consumed regardless of whether they trigger an
    // accelerator to prevent windows from seeing unexpected key up events.
    WmShell::Get()->accelerator_controller()->Process(accelerator);
    return true;
  }
  if (!ShouldProcessAcceleratorNow(target, key_event, accelerator))
    return false;
  return WmShell::Get()->accelerator_controller()->Process(accelerator);
}

void AcceleratorRouter::RecordSearchKeyStats(
    const ui::Accelerator& accelerator) {
  if (accelerator.IsCmdDown()) {
    if (search_key_state_ == RELEASED) {
      search_key_state_ = PRESSED;
      search_key_pressed_timestamp_ = base::TimeTicks::Now();
    }

    if (accelerator.key_code() != ui::KeyboardCode::VKEY_COMMAND &&
        search_key_state_ == PRESSED) {
      search_key_state_ = RECORDED;
      UMA_HISTOGRAM_TIMES(
          "Keyboard.Shortcuts.CrosSearchKeyDelay",
          base::TimeTicks::Now() - search_key_pressed_timestamp_);
    }
  } else {
    search_key_state_ = RELEASED;
  }
}

bool AcceleratorRouter::CanConsumeSystemKeys(WmWindow* target,
                                             const ui::KeyEvent& event) {
  // Uses the top level window so if the target is a web contents window the
  // containing parent window will be checked for the property.
  WmWindow* top_level = target->GetToplevelWindowForFocus();
  return top_level && top_level->GetWindowState()->can_consume_system_keys();
}

bool AcceleratorRouter::ShouldProcessAcceleratorNow(
    WmWindow* target,
    const ui::KeyEvent& event,
    const ui::Accelerator& accelerator) {
  // Callers should never supply null.
  DCHECK(target);
  // On ChromeOS, If the accelerator is Search+<key(s)> then it must never be
  // intercepted by apps or windows.
  if (accelerator.IsCmdDown())
    return true;

  if (base::ContainsValue(WmShell::Get()->GetAllRootWindows(), target))
    return true;

  AcceleratorController* accelerator_controller =
      WmShell::Get()->accelerator_controller();

  // Reserved accelerators (such as Power button) always have a prority.
  if (accelerator_controller->IsReserved(accelerator))
    return true;

  // A full screen window has a right to handle all key events including the
  // reserved ones.
  WmWindow* top_level = target->GetToplevelWindowForFocus();
  if (top_level && top_level->GetWindowState()->IsFullscreen()) {
    // On ChromeOS, fullscreen windows are either browser or apps, which
    // send key events to a web content first, then will process keys
    // if the web content didn't consume them.
    return false;
  }

  // Handle preferred accelerators (such as ALT-TAB) before sending
  // to the target.
  if (accelerator_controller->IsPreferred(accelerator))
    return true;

  return WmShell::Get()->GetAppListTargetVisibility();
}

}  // namespace ash
