// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_STATUS_VIEW_H_
#define ASH_SYSTEM_POWER_POWER_STATUS_VIEW_H_
#pragma once

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

  explicit PowerStatusView(ViewType view_type);
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

  // labels used only for VIEW_NOTIFICATION.
  views::Label* status_label_;
  views::Label* time_label_;

  // labels used only for VIEW_DEFAULT.
  views::Label* time_status_label_;

  views::ImageView* icon_;
  ViewType view_type_;

  PowerSupplyStatus supply_status_;

  DISALLOW_COPY_AND_ASSIGN(PowerStatusView);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_STATUS_VIEW_H_
