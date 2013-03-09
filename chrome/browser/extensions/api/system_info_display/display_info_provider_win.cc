// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_display/display_info_provider.h"

#include <windows.h>

#include "base/utf_string_conversions.h"
#include "ui/base/win/dpi.h"
#include "ui/gfx/display.h"
#include "ui/gfx/size.h"

namespace extensions {

using api::system_info_display::Bounds;
using api::system_info_display::DisplayUnitInfo;

namespace {

BOOL CALLBACK EnumMonitorCallback(HMONITOR monitor,
                                  HDC hdc,
                                  LPRECT rect,
                                  LPARAM data) {
  DisplayInfo* display_info =
      reinterpret_cast<DisplayInfo*>(data);
  DCHECK(display_info);

  linked_ptr<DisplayUnitInfo> unit(new DisplayUnitInfo);

  MONITORINFOEX monitor_info;
  ZeroMemory(&monitor_info, sizeof(MONITORINFOEX));
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(monitor, &monitor_info);

  DISPLAY_DEVICE device;
  device.cb = sizeof(device);
  if (!EnumDisplayDevices(monitor_info.szDevice, 0, &device, 0))
    return FALSE;

  gfx::Display display(0, gfx::Rect(monitor_info.rcMonitor));
  display.set_work_area(gfx::Rect(monitor_info.rcWork));

  gfx::Size dpi(ui::GetDPI());
  unit->id = WideToUTF8(device.DeviceID);
  unit->name = WideToUTF8(device.DeviceString);
  unit->is_primary = monitor_info.dwFlags & MONITORINFOF_PRIMARY ? true : false;

  // TODO(hongbo): Figure out how to determine whether the display monitor is
  // internal or not.
  unit->is_internal = false;
  unit->is_enabled = device.StateFlags & DISPLAY_DEVICE_ACTIVE ? true : false;
  unit->dpi_x = dpi.width();
  unit->dpi_y = dpi.height();
  unit->bounds.left = display.bounds().x();
  unit->bounds.top = display.bounds().y();
  unit->bounds.width = display.bounds().width();
  unit->bounds.height = display.bounds().height();
  unit->work_area.left = display.work_area().x();
  unit->work_area.top = display.work_area().y();
  unit->work_area.width = display.work_area().width();
  unit->work_area.height = display.work_area().height();
  display_info->push_back(unit);

  return TRUE;
}

}  // namespace

bool DisplayInfoProvider::QueryInfo(DisplayInfo* info) {
  DCHECK(info);
  info->clear();

  if (EnumDisplayMonitors(NULL, NULL, EnumMonitorCallback,
        reinterpret_cast<LPARAM>(info)))
    return true;
  return false;
}

}  // namespace extensions
