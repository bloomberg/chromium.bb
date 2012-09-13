// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_STATUS_VIEW_H_
#define ASH_SYSTEM_POWER_POWER_STATUS_VIEW_H_

#include "ash/system/power/power_supply_status.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}

namespace ash {
namespace internal {

class PowerStatusView : public views::View {
 public:
  enum ViewType {
    VIEW_DEFAULT,
    VIEW_NOTIFICATION
  };

  PowerStatusView(ViewType view_type, bool default_view_right_align);
  virtual ~PowerStatusView() {}

  void UpdatePowerStatus(const PowerSupplyStatus& status);

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;

 private:
  void LayoutDefaultView();
  void LayoutNotificationView();
  void UpdateText();
  void UpdateIcon();
  void Update();
  void UpdateTextForDefaultView();
  void UpdateTextForNotificationView();

  // Overridden from views::View.
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;

  // Layout default view UI items on the right side of system tray pop up item
  // if true; otherwise, layout the UI items on the left side.
  bool default_view_right_align_;

  // Labels used only for VIEW_NOTIFICATION.
  views::Label* status_label_;
  views::Label* time_label_;

  // Labels used only for VIEW_DEFAULT.
  views::Label* time_status_label_;

  // Battery status indicator icon.
  views::ImageView* icon_;

  // Index of the current icon in the icon array image, or -1 if unknown.
  int icon_image_index_;

  ViewType view_type_;

  PowerSupplyStatus supply_status_;

  DISALLOW_COPY_AND_ASSIGN(PowerStatusView);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_STATUS_VIEW_H_
