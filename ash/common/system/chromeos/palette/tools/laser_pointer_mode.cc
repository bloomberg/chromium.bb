// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/tools/laser_pointer_mode.h"

#include "ash/common/palette_delegate.h"
#include "ash/common/system/chromeos/palette/palette_ids.h"
#include "ash/common/system/chromeos/palette/tools/laser_pointer_view.h"
#include "ash/common/wm_shell.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/cursor_manager.h"

namespace ash {
namespace {

// A point gets removed from the collection if it is older than
// |kPointLifeDurationMs|.
const int kPointLifeDurationMs = 200;

// When no move events are being recieved we add a new point every
// |kAddStationaryPointsDelayMs| so that points older than
// |kPointLifeDurationMs| can get removed.
const int kAddStationaryPointsDelayMs = 5;

}  // namespace

LaserPointerMode::LaserPointerMode(Delegate* delegate)
    : CommonPaletteTool(delegate) {
  laser_pointer_view_.reset(new LaserPointerView(
      base::TimeDelta::FromMilliseconds(kPointLifeDurationMs)));
  timer_.reset(new base::Timer(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kAddStationaryPointsDelayMs),
      base::Bind(&LaserPointerMode::AddStationaryPoint, base::Unretained(this)),
      true));
  WmShell::Get()->AddPointerWatcher(this,
                                    views::PointerWatcherEventTypes::DRAGS);
}

LaserPointerMode::~LaserPointerMode() {
  OnDisable();
  WmShell::Get()->RemovePointerWatcher(this);
}

PaletteGroup LaserPointerMode::GetGroup() const {
  return PaletteGroup::MODE;
}

PaletteToolId LaserPointerMode::GetToolId() const {
  return PaletteToolId::LASER_POINTER;
}

void LaserPointerMode::OnEnable() {
  CommonPaletteTool::OnEnable();

  WmShell::Get()->palette_delegate()->OnLaserPointerEnabled();
  laser_pointer_view_->AddNewPoint(current_mouse_location_);
}

void LaserPointerMode::OnDisable() {
  CommonPaletteTool::OnDisable();

  WmShell::Get()->palette_delegate()->OnLaserPointerDisabled();
  StopTimer();
  laser_pointer_view_->Stop();
}

gfx::VectorIconId LaserPointerMode::GetActiveTrayIcon() {
  return gfx::VectorIconId::PALETTE_TRAY_ICON_LASER_POINTER;
}

gfx::VectorIconId LaserPointerMode::GetPaletteIconId() {
  return gfx::VectorIconId::PALETTE_MODE_LASER_POINTER;
}

views::View* LaserPointerMode::CreateView() {
  return CreateDefaultView(
      l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_LASER_POINTER_MODE));
}

void LaserPointerMode::StopTimer() {
  timer_repeat_count_ = 0;
  timer_->Stop();
}

void LaserPointerMode::AddStationaryPoint() {
  laser_pointer_view_->AddNewPoint(current_mouse_location_);
  // We can stop repeating the timer once the mouse has been stationary for
  // longer than the life of a point.
  if (timer_repeat_count_++ * kAddStationaryPointsDelayMs >=
      kPointLifeDurationMs) {
    StopTimer();
  }
}

void LaserPointerMode::OnPointerEventObserved(
    const ui::PointerEvent& event,
    const gfx::Point& location_in_screen,
    views::Widget* target) {
  // TODO(sammiequon): Add support for pointer drags. See crbug.com/640410.
  if (event.type() == ui::ET_POINTER_MOVED &&
      event.pointer_details().pointer_type ==
          ui::EventPointerType::POINTER_TYPE_PEN) {
    current_mouse_location_ = location_in_screen;
    if (enabled()) {
      laser_pointer_view_->AddNewPoint(current_mouse_location_);
      timer_repeat_count_ = 0;
      if (!timer_->IsRunning())
        timer_->Reset();
    }
  }
}
}  // namespace ash
