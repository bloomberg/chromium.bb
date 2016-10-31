// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/date/system_info_default_view.h"

#include "ash/common/system/date/date_view.h"
#include "ash/common/system/tray/tray_utils.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

#if defined(OS_CHROMEOS)
#include "ash/common/system/chromeos/power/power_status.h"
#include "ash/common/system/chromeos/power/power_status_view.h"
#endif  // defined(OS_CHROMEOS)

namespace ash {

SystemInfoDefaultView::SystemInfoDefaultView(SystemTrayItem* owner,
                                             LoginStatus login)
    : date_view_(nullptr) {
  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 4, 0, 0);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(box_layout);

  date_view_ = new tray::DateView(owner);
  AddChildView(date_view_);

#if defined(OS_CHROMEOS)
  if (PowerStatus::Get()->IsBatteryPresent()) {
    // TODO(tdanderson): Align separator with the nearest separator in the
    // tiles row above.
    AddChildView(CreateVerticalSeparator());
    power_status_view_ = new ash::PowerStatusView(false);
    AddChildView(power_status_view_);
  }
#endif  // defined(OS_CHROMEOS)

  if (CanOpenWebUISettings(login))
    date_view_->SetAction(tray::DateView::DateAction::SHOW_DATE_SETTINGS);
}

SystemInfoDefaultView::~SystemInfoDefaultView() {}

tray::DateView* SystemInfoDefaultView::GetDateView() {
  return date_view_;
}

const tray::DateView* SystemInfoDefaultView::GetDateView() const {
  return date_view_;
}

}  // namespace ash
