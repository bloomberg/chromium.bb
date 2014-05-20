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
  enum ViewType {
    VIEW_DEFAULT,
    VIEW_NOTIFICATION
  };

  PowerStatusView(ViewType view_type, bool default_view_right_align);
  virtual ~PowerStatusView();

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual int GetHeightForWidth(int width) const OVERRIDE;
  virtual void Layout() OVERRIDE;

  // Overridden from PowerStatus::Observer.
  virtual void OnPowerStatusChanged() OVERRIDE;

 private:
  friend class PowerStatusDefaultViewTest;
  friend class PowerStatusNotificationViewTest;

  void LayoutDefaultView();
  void LayoutNotificationView();
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
  views::Label* percentage_label_;

  // Battery status indicator icon.
  views::ImageView* icon_;

  ViewType view_type_;

  DISALLOW_COPY_AND_ASSIGN(PowerStatusView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_POWER_POWER_STATUS_VIEW_H_
