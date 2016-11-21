// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/date/system_info_default_view.h"

#include "ash/common/system/date/date_view.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/tray/tray_popup_utils.h"
#include "ash/common/system/tray/tri_view.h"
#include "base/memory/ptr_util.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

#if defined(OS_CHROMEOS)
#include "ash/common/system/chromeos/power/power_status.h"
#include "ash/common/system/chromeos/power/power_status_view.h"
#endif  // defined(OS_CHROMEOS)

namespace ash {

// The minimum number of menu button widths that the date view should span
// horizontally.
const int kMinNumTileWidths = 2;

// The maximum number of menu button widths that the date view should span
// horizontally.
const int kMaxNumTileWidths = 3;

SystemInfoDefaultView::SystemInfoDefaultView(SystemTrayItem* owner,
                                             LoginStatus login)
    : date_view_(nullptr),
      tri_view_(TrayPopupUtils::CreateMultiTargetRowView()) {
  AddChildView(tri_view_);
  SetLayoutManager(new views::FillLayout);

  date_view_ = new tray::DateView(owner);
  tri_view_->AddView(TriView::Container::START, date_view_);

#if defined(OS_CHROMEOS)
  if (PowerStatus::Get()->IsBatteryPresent()) {
    power_status_view_ = new ash::PowerStatusView(false);
    std::unique_ptr<views::BoxLayout> box_layout =
        base::MakeUnique<views::BoxLayout>(views::BoxLayout::kHorizontal, 0, 0,
                                           0);
    box_layout->set_cross_axis_alignment(
        views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
    box_layout->set_inside_border_insets(
        gfx::Insets(0, 0, 0, kTrayPopupLabelRightPadding));
    tri_view_->SetContainerLayout(TriView::Container::CENTER,
                                  std::move(box_layout));

    tri_view_->AddView(TriView::Container::CENTER,
                       TrayPopupUtils::CreateVerticalSeparator());
    tri_view_->AddView(TriView::Container::CENTER, power_status_view_);
  }
#endif  // defined(OS_CHROMEOS)
  tri_view_->SetContainerVisible(TriView::Container::END, false);

  if (TrayPopupUtils::CanOpenWebUISettings(login))
    date_view_->SetAction(tray::DateView::DateAction::SHOW_DATE_SETTINGS);
}

SystemInfoDefaultView::~SystemInfoDefaultView() {}

tray::DateView* SystemInfoDefaultView::GetDateView() {
  return date_view_;
}

const tray::DateView* SystemInfoDefaultView::GetDateView() const {
  return date_view_;
}

void SystemInfoDefaultView::Layout() {
  gfx::Size min_start_size = tri_view_->GetMinSize(TriView::Container::START);
  min_start_size.set_width(
      CalculateDateViewWidth(date_view_->GetPreferredSize().width()));
  tri_view_->SetMinSize(TriView::Container::START, min_start_size);

  views::View::Layout();
}

int SystemInfoDefaultView::CalculateDateViewWidth(int preferred_width) {
  const float snap_to_width = kSeparatorWidth + kMenuButtonSize;
  int num_extra_tile_widths = 0;
  if (preferred_width > kMenuButtonSize) {
    const float extra_width = preferred_width - kMenuButtonSize;
    const float preferred_width_ratio = extra_width / snap_to_width;
    num_extra_tile_widths = std::ceil(preferred_width_ratio);
  }
  num_extra_tile_widths =
      std::max(kMinNumTileWidths - 1,
               std::min(num_extra_tile_widths, kMaxNumTileWidths - 1));

  return kMenuButtonSize + num_extra_tile_widths * snap_to_width;
}

}  // namespace ash
