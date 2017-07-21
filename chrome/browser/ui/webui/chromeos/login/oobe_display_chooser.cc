// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/oobe_display_chooser.h"

#include <stdint.h>

#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "base/stl_util.h"
#include "content/public/browser/browser_thread.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/touchscreen_device.h"

using content::BrowserThread;

namespace chromeos {

namespace {

bool TouchSupportAvailable(const display::Display& display) {
  return display.touch_support() ==
         display::Display::TouchSupport::TOUCH_SUPPORT_AVAILABLE;
}

// TODO(felixe): More context at crbug.com/738885
const uint16_t kDeviceIds[] = {0x0457, 0x266e};

}  // namespace

OobeDisplayChooser::OobeDisplayChooser() : weak_ptr_factory_(this) {}

OobeDisplayChooser::~OobeDisplayChooser() {}

void OobeDisplayChooser::TryToPlaceUiOnTouchDisplay() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Don't (potentially) queue a second task to run MoveToTouchDisplay if one
  // already is queued.
  if (weak_ptr_factory_.HasWeakPtrs())
    return;

  display::Display primary_display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  if (primary_display.is_valid() && !TouchSupportAvailable(primary_display)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&OobeDisplayChooser::MoveToTouchDisplay,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void OobeDisplayChooser::MoveToTouchDisplay() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const ui::DeviceDataManager* device_manager =
      ui::DeviceDataManager::GetInstance();
  for (const ui::TouchscreenDevice& device :
       device_manager->GetTouchscreenDevices()) {
    if (!base::ContainsValue(kDeviceIds, device.vendor_id))
      continue;

    int64_t display_id =
        device_manager->GetTargetDisplayForTouchDevice(device.id);
    if (display_id == display::kInvalidDisplayId)
      continue;

    ash::Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(
        display_id);
    break;
  }
}

}  // namespace chromeos
