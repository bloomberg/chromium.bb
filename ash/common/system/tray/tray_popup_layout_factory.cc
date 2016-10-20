// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_popup_layout_factory.h"

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/system/tray/three_view_layout.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Creates a layout manager that positions Views vertically. The Views will be
// stretched horizontally and centered vertically.
std::unique_ptr<views::LayoutManager> CreateDefaultCenterLayoutManager() {
  // TODO(bruthig): Use constants instead of magic numbers.
  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 4, 8, 4);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  return std::unique_ptr<views::LayoutManager>(box_layout);
}

// Creates a layout manager that positions Views horizontally. The Views will be
// centered along the horizontal and vertical axis.
std::unique_ptr<views::LayoutManager> CreateDefaultEndsLayoutManager() {
  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  return std::unique_ptr<views::LayoutManager>(box_layout);
}

}  // namespace

ThreeViewLayout* TrayPopupLayoutFactory::InstallDefaultLayout(
    views::View* host) {
  ThreeViewLayout* layout = new ThreeViewLayout(0 /* padding_between_items */);

  // TODO(bruthig): views::BoxLayout::SetFlexForView() fails on a DCHECK() if it
  // has not yet been installed on a View. Evaluate whether this is necessary or
  // if it can be removed. This would allow us to drop the |host| parameter from
  // this function.
  host->SetLayoutManager(layout);

  layout->SetInsets(gfx::Insets(0, GetTrayConstant(TRAY_POPUP_ITEM_LEFT_INSET),
                                0,
                                GetTrayConstant(TRAY_POPUP_ITEM_RIGHT_INSET)));
  layout->SetMinCrossAxisSize(GetTrayConstant(TRAY_POPUP_ITEM_HEIGHT));

  ConfigureDefaultLayout(layout, ThreeViewLayout::Container::START);
  ConfigureDefaultLayout(layout, ThreeViewLayout::Container::CENTER);
  ConfigureDefaultLayout(layout, ThreeViewLayout::Container::END);

  return layout;
}

void TrayPopupLayoutFactory::ConfigureDefaultLayout(
    ThreeViewLayout* layout,
    ThreeViewLayout::Container container) {
  switch (container) {
    case ThreeViewLayout::Container::START:
      layout->SetLayoutManager(ThreeViewLayout::Container::START,
                               CreateDefaultEndsLayoutManager());
      layout->SetMinSize(
          ThreeViewLayout::Container::START,
          gfx::Size(GetTrayConstant(TRAY_POPUP_ITEM_MIN_START_WIDTH), 0));
      break;
    case ThreeViewLayout::Container::CENTER:
      layout->SetLayoutManager(ThreeViewLayout::Container::CENTER,
                               CreateDefaultCenterLayoutManager());
      layout->SetFlexForContainer(ThreeViewLayout::Container::CENTER, 1.f);
      break;
    case ThreeViewLayout::Container::END:
      layout->SetLayoutManager(ThreeViewLayout::Container::END,
                               CreateDefaultEndsLayoutManager());
      layout->SetMinSize(
          ThreeViewLayout::Container::END,
          gfx::Size(GetTrayConstant(TRAY_POPUP_ITEM_MIN_END_WIDTH), 0));
      break;
  }
}

}  // namespace ash
