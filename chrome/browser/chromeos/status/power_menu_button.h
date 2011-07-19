// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_POWER_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_POWER_MENU_BUTTON_H_
#pragma once

#include "chrome/browser/chromeos/cros/power_library.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "views/controls/menu/menu_delegate.h"
#include "views/controls/menu/view_menu_delegate.h"

namespace base {
class TimeDelta;
}

class SkBitmap;

namespace chromeos {

// The power menu button in the status area.
// This class will handle getting the power status and populating the menu.
class PowerMenuButton : public StatusAreaButton,
                        public views::MenuDelegate,
                        public views::ViewMenuDelegate,
                        public PowerLibrary::Observer {
 public:
  explicit PowerMenuButton(StatusAreaHost* host);
  virtual ~PowerMenuButton();

  // views::MenuDelegate implementation.
  virtual std::wstring GetLabel(int id) const;
  virtual bool IsCommandEnabled(int id) const;

  // PowerLibrary::Observer implementation.
  virtual void PowerChanged(PowerLibrary* obj);
  virtual void SystemResumed() {}

  int icon_id() const { return icon_id_; }

 protected:
  virtual int icon_width();

 private:
  // views::View
  virtual void OnLocaleChanged() OVERRIDE;

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Format strings with power status
  string16 GetBatteryPercentageText() const;
  string16 GetBatteryIsChargedText() const;

  // Update the power icon and menu label info depending on the power status.
  void UpdateIconAndLabelInfo();

  // Update the menu entries.
  void UpdateMenu();

  // The number of power images.
  static const int kNumPowerImages;

  // Stored data gathered from CrosLibrary::PowerLibrary.
  bool battery_is_present_;
  bool line_power_on_;
  bool battery_fully_charged_;
  double battery_percentage_;
  base::TimeDelta battery_time_to_full_;
  base::TimeDelta battery_time_to_empty_;

  // The currently showing icon bitmap id.
  int icon_id_;

  // The power menu. This needs to be initialized last since it calls into
  // GetLabelAt() during construction.
  scoped_ptr<views::MenuItemView> menu_;

  DISALLOW_COPY_AND_ASSIGN(PowerMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_POWER_MENU_BUTTON_H_
