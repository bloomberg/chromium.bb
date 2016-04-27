// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/scoped_disable_internal_mouse_and_keyboard_x11.h"

#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>

#include <memory>
#include <set>
#include <utility>

#include "ash/display/window_tree_host_manager.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "base/strings/string_util.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/device_list_cache_x11.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11_types.h"

namespace ash {

namespace {

// The name of the xinput device corresponding to the internal touchpad.
const char kInternalTouchpadName[] = "Elan Touchpad";

// Repeated key events have their source set to the core keyboard device.
// These must be disabled also until http://crbug.com/402898 is resolved.
const char kCoreKeyboardName[] = "Virtual core keyboard";

// Device id used to indicate that a device has not been detected.
const int kDeviceIdNone = -1;

gfx::Point GetMouseLocationInScreen() {
  return aura::Env::GetInstance()->last_mouse_location();
}

void SetMouseLocationInScreen(const gfx::Point& screen_location) {
  display::Display display =
      ash::ScreenUtil::FindDisplayContainingPoint(screen_location);
  if (!display.is_valid())
    return;
  aura::Window* root_window = Shell::GetInstance()
                                  ->window_tree_host_manager()
                                  ->GetRootWindowForDisplayId(display.id());
  gfx::Point host_location(screen_location);
  aura::client::ScreenPositionClient* client =
      aura::client::GetScreenPositionClient(root_window);
  if (client)
    client->ConvertPointFromScreen(root_window, &host_location);
  root_window->GetHost()->MoveCursorTo(host_location);
}

}  // namespace

ScopedDisableInternalMouseAndKeyboardX11::
    ScopedDisableInternalMouseAndKeyboardX11()
    : touchpad_device_id_(kDeviceIdNone),
      keyboard_device_id_(kDeviceIdNone),
      core_keyboard_device_id_(kDeviceIdNone),
      last_mouse_location_(GetMouseLocationInScreen()) {

  ui::DeviceDataManagerX11* device_data_manager =
      ui::DeviceDataManagerX11::GetInstance();
  if (device_data_manager->IsXInput2Available()) {
    const XIDeviceList& xi_dev_list =
        ui::DeviceListCacheX11::GetInstance()->GetXI2DeviceList(
            gfx::GetXDisplay());
    for (int i = 0; i < xi_dev_list.count; ++i) {
      std::string device_name(xi_dev_list[i].name);
      base::TrimWhitespaceASCII(device_name, base::TRIM_TRAILING, &device_name);
      if (device_name == kInternalTouchpadName) {
        if (device_data_manager->IsDeviceEnabled(xi_dev_list[i].deviceid)) {
          // If the touchpad is already disabled we will do nothing about it.
          // This will result in doing nothing in the destructor as well since
          // |touchpad_device_id_| will remain |kDeviceIdNone|.
          touchpad_device_id_ = xi_dev_list[i].deviceid;
          device_data_manager->DisableDevice(touchpad_device_id_);
          Shell::GetInstance()->cursor_manager()->HideCursor();
        }
      } else if (device_name == kCoreKeyboardName) {
        core_keyboard_device_id_ = xi_dev_list[i].deviceid;
        device_data_manager->DisableDevice(core_keyboard_device_id_);
      }
    }

    for (const ui::KeyboardDevice& device :
         device_data_manager->keyboard_devices()) {
      if (device.type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
        keyboard_device_id_ = device.id;
        device_data_manager->DisableDevice(keyboard_device_id_);
        break;
      }
    }
  }
  // Allow the accessible keys present on the side of some devices to continue
  // working.
  std::unique_ptr<std::set<ui::KeyboardCode>> excepted_keys(
      new std::set<ui::KeyboardCode>);
  excepted_keys->insert(ui::VKEY_VOLUME_DOWN);
  excepted_keys->insert(ui::VKEY_VOLUME_UP);
  excepted_keys->insert(ui::VKEY_POWER);
  device_data_manager->SetDisabledKeyboardAllowedKeys(std::move(excepted_keys));
  ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
}

ScopedDisableInternalMouseAndKeyboardX11::
    ~ScopedDisableInternalMouseAndKeyboardX11() {
  ui::DeviceDataManagerX11* device_data_manager =
      static_cast<ui::DeviceDataManagerX11*>(
          ui::DeviceDataManager::GetInstance());
  if (touchpad_device_id_ != kDeviceIdNone) {
    device_data_manager->EnableDevice(touchpad_device_id_);
    Shell::GetInstance()->cursor_manager()->ShowCursor();
  }
  if (keyboard_device_id_ != kDeviceIdNone)
    device_data_manager->EnableDevice(keyboard_device_id_);
  if (core_keyboard_device_id_ != kDeviceIdNone)
    device_data_manager->EnableDevice(core_keyboard_device_id_);
  device_data_manager->SetDisabledKeyboardAllowedKeys(
      std::unique_ptr<std::set<ui::KeyboardCode>>());
  ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
}

void ScopedDisableInternalMouseAndKeyboardX11::WillProcessEvent(
    const ui::PlatformEvent& event) {
}

void ScopedDisableInternalMouseAndKeyboardX11::DidProcessEvent(
    const ui::PlatformEvent& event) {
  if (event->type != GenericEvent)
    return;
  XIDeviceEvent* xievent =
      static_cast<XIDeviceEvent*>(event->xcookie.data);
  ui::DeviceDataManagerX11* device_data_manager =
      static_cast<ui::DeviceDataManagerX11*>(
          ui::DeviceDataManager::GetInstance());
  if (xievent->evtype != XI_Motion ||
      device_data_manager->IsFlingEvent(*event) ||
      device_data_manager->IsScrollEvent(*event) ||
      device_data_manager->IsCMTMetricsEvent(*event)) {
    return;
  }
  if (xievent->sourceid == touchpad_device_id_) {
    // The cursor will have already moved even though the move event will be
    // blocked. Move the mouse cursor back to its last known location resulting
    // from an external mouse to prevent the internal touchpad from moving it.
    SetMouseLocationInScreen(last_mouse_location_);
  } else {
    // Track the last location seen from an external mouse event.
    last_mouse_location_ = GetMouseLocationInScreen();
  }
}

}  // namespace ash
