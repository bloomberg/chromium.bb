// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_POWER_POWER_STATUS_VIEW_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_POWER_POWER_STATUS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/common/system/chromeos/power/power_status.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}

namespace ash {

class ASH_EXPORT PowerStatusView : public views::View,
                                   public PowerStatus::Observer {
 public:
  explicit PowerStatusView(bool default_view_right_align);
  ~PowerStatusView() override;

  // views::View:
  void Layout() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

 private:
  friend class PowerStatusViewTest;

  void LayoutView();
  void UpdateText();

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;

  // Layout default view UI items on the right side of system tray pop up item
  // if true; otherwise, layout the UI items on the left side.
  bool default_view_right_align_;

  views::Label* percentage_label_;
  views::Label* separator_label_;
  views::Label* time_status_label_;

  // Battery status indicator icon. Unused in material design.
  views::ImageView* icon_;

  // Information about the image last used to update |icon_|. Cached to avoid
  // unnecessary updates (http://crbug.com/589348).
  PowerStatus::BatteryImageInfo previous_battery_image_info_;

  // Only used in material design.
  base::string16 accessible_name_;

  DISALLOW_COPY_AND_ASSIGN(PowerStatusView);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_POWER_POWER_STATUS_VIEW_H_
