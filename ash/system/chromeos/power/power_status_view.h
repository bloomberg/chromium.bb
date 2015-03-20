// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_POWER_POWER_STATUS_VIEW_H_
#define ASH_SYSTEM_CHROMEOS_POWER_POWER_STATUS_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/chromeos/power/power_status.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}

namespace ash {

class ASH_EXPORT PowerStatusView : public views::View,
                                   public PowerStatus::Observer {
 public:
  PowerStatusView(bool default_view_right_align);
  ~PowerStatusView() override;

  // Overridden from views::View.
  gfx::Size GetPreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void Layout() override;

  // Overridden from PowerStatus::Observer.
  void OnPowerStatusChanged() override;

 private:
  friend class PowerStatusViewTest;

  void LayoutView();
  void UpdateText();

  // Overridden from views::View.
  void ChildPreferredSizeChanged(views::View* child) override;

  // Layout default view UI items on the right side of system tray pop up item
  // if true; otherwise, layout the UI items on the left side.
  bool default_view_right_align_;

  views::Label* time_status_label_;
  views::Label* percentage_label_;

  // Battery status indicator icon.
  views::ImageView* icon_;

  DISALLOW_COPY_AND_ASSIGN(PowerStatusView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_POWER_POWER_STATUS_VIEW_H_
