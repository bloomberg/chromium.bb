// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/internal_input_device_list_x11.h"

#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>

#include "base/strings/string_util.h"
#include "ui/events/event.h"
#include "ui/events/x/device_data_manager.h"
#include "ui/events/x/device_list_cache_x.h"
#include "ui/gfx/x/x11_types.h"

namespace ash {

namespace {

// The name of the xinput device corresponding to the internal touchpad.
const char kInternalTouchpadName[] = "Elan Touchpad";

// The name of the xinput device corresponding to the internal keyboard.
const char kInternalKeyboardName[] = "AT Translated Set 2 keyboard";

}  // namespace

InternalInputDeviceListX11::InternalInputDeviceListX11() {
  if (ui::DeviceDataManager::GetInstance()->IsXInput2Available()) {
    XIDeviceList xi_dev_list = ui::DeviceListCacheX::GetInstance()->
        GetXI2DeviceList(gfx::GetXDisplay());
    for (int i = 0; i < xi_dev_list.count; ++i) {
      std::string device_name(xi_dev_list[i].name);
      base::TrimWhitespaceASCII(device_name, base::TRIM_TRAILING, &device_name);
      if (device_name == kInternalTouchpadName ||
          device_name == kInternalKeyboardName)
        internal_device_ids_.insert(xi_dev_list[i].deviceid);
    }
  }
}

InternalInputDeviceListX11::~InternalInputDeviceListX11() {
}

bool InternalInputDeviceListX11::IsEventFromInternalDevice(
    const ui::Event* event) {
  if (!event->HasNativeEvent())
    return false;
  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(
      event->native_event()->xcookie.data);
  return internal_device_ids_.find(xiev->sourceid) !=
      internal_device_ids_.end();
}

}  // namespace ash
