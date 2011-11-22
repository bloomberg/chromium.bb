// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_POWER_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_POWER_MENU_BUTTON_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "views/controls/menu/menu_delegate.h"
#include "views/controls/menu/view_menu_delegate.h"

namespace base {
class TimeDelta;
}

namespace views {
class MenuRunner;
}

namespace chromeos {

class StatusAreaBubbleContentView;

// The power menu button in the status area.
// This class will handle getting the power status and populating the menu.
class PowerMenuButton : public StatusAreaButton,
                        public views::MenuDelegate,
                        public views::ViewMenuDelegate,
                        public PowerManagerClient::Observer {
 public:
  explicit PowerMenuButton(StatusAreaButton::Delegate* delegate);
  virtual ~PowerMenuButton();

  // views::MenuDelegate implementation.
  virtual string16 GetLabel(int id) const OVERRIDE;

  // PowerLibrary::Observer implementation.
  virtual void PowerChanged(const PowerSupplyStatus& power_status) OVERRIDE;
  virtual void SystemResumed() {}

 protected:
  virtual int icon_width() OVERRIDE;

 private:
  // views::View
  virtual void OnLocaleChanged() OVERRIDE;

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt) OVERRIDE;

  // Format strings with power status
  string16 GetBatteryIsChargedText() const;

  // Update the power icon and menu label info depending on the power status.
  void UpdateIconAndLabelInfo();

  // Update status view
  void UpdateStatusView();

  // Update Battery time. Try to make it monotonically decreasing unless
  // there's a large delta.
  void UpdateBatteryTime(base::TimeDelta* previous,
                         const base::TimeDelta& current);

  // Stored data gathered from CrosLibrary::PowerLibrary.
  bool battery_is_present_;
  bool line_power_on_;
  double battery_percentage_;
  int battery_index_;
  PowerSupplyStatus power_status_;

  base::TimeDelta battery_time_to_full_;
  base::TimeDelta battery_time_to_empty_;

  // The currently showing status view. NULL if menu is not being displayed.
  StatusAreaBubbleContentView* status_;

  // If non-null the menu is showing.
  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(PowerMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_POWER_MENU_BUTTON_H_
