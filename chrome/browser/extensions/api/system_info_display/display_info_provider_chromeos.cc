// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info_display/display_info_provider.h"

#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "base/strings/string_number_conversions.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"

using ash::internal::DisplayManager;

namespace extensions {

namespace {

// TODO(hshi): determine the DPI of the screen.
const float kDpi96 = 96.0;

}  // namespace

using api::system_info_display::Bounds;
using api::system_info_display::DisplayUnitInfo;

bool DisplayInfoProvider::QueryInfo(DisplayInfo* info) {
  DCHECK(info);
  info->clear();

  DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();
  DCHECK(display_manager);

  int64 primary_id = ash::Shell::GetScreen()->GetPrimaryDisplay().id();
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    linked_ptr<DisplayUnitInfo> unit(new DisplayUnitInfo());
    const gfx::Display* display = display_manager->GetDisplayAt(i);
    const gfx::Rect& bounds = display->bounds();
    const gfx::Rect& work_area = display->work_area();
    const float dpi = display->device_scale_factor() * kDpi96;
    unit->id = base::Int64ToString(display->id());
    unit->name = display_manager->GetDisplayNameForId(display->id());
    unit->is_primary = (display->id() == primary_id);
    unit->is_internal = display->IsInternal();
    unit->is_enabled = true;
    unit->dpi_x = dpi;
    unit->dpi_y = dpi;
    unit->bounds.left = bounds.x();
    unit->bounds.top = bounds.y();
    unit->bounds.width = bounds.width();
    unit->bounds.height = bounds.height();
    unit->work_area.left = work_area.x();
    unit->work_area.top = work_area.y();
    unit->work_area.width = work_area.width();
    unit->work_area.height = work_area.height();
    info->push_back(unit);
  }

  return true;
}

}  // namespace extensions
