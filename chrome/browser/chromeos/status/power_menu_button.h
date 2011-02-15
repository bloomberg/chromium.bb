// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_POWER_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_POWER_MENU_BUTTON_H_
#pragma once

#include "chrome/browser/chromeos/cros/power_library.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "ui/base/models/menu_model.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/view_menu_delegate.h"

namespace base {
class TimeDelta;
}

class SkBitmap;

namespace chromeos {

// The power menu button in the status area.
// This class will handle getting the power status and populating the menu.
class PowerMenuButton : public StatusAreaButton,
                        public views::ViewMenuDelegate,
                        public ui::MenuModel,
                        public PowerLibrary::Observer {
 public:
  PowerMenuButton();
  virtual ~PowerMenuButton();

  // ui::MenuModel implementation.
  virtual bool HasIcons() const  { return false; }
  virtual int GetItemCount() const;
  virtual ui::MenuModel::ItemType GetTypeAt(int index) const;
  virtual int GetCommandIdAt(int index) const { return index; }
  virtual string16 GetLabelAt(int index) const;
  virtual bool IsItemDynamicAt(int index) const { return true; }
  virtual bool GetAcceleratorAt(int index,
      ui::Accelerator* accelerator) const { return false; }
  virtual bool IsItemCheckedAt(int index) const { return false; }
  virtual int GetGroupIdAt(int index) const { return 0; }
  virtual bool GetIconAt(int index, SkBitmap* icon) const { return false; }
  virtual ui::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const {
    return NULL;
  }
  virtual bool IsEnabledAt(int index) const { return false; }
  virtual ui::MenuModel* GetSubmenuModelAt(int index) const { return NULL; }
  virtual void HighlightChangedTo(int index) {}
  virtual void ActivatedAt(int index) {}
  virtual void MenuWillShow() {}

  // PowerLibrary::Observer implementation.
  virtual void PowerChanged(PowerLibrary* obj);
  virtual void SystemResumed() {}

  int icon_id() const { return icon_id_; }

 protected:
  virtual int icon_width() { return 26; }

 private:
  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Update the power icon and menu label info depending on the power status.
  void UpdateIconAndLabelInfo();

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
  views::Menu2 power_menu_;

  DISALLOW_COPY_AND_ASSIGN(PowerMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_POWER_MENU_BUTTON_H_
