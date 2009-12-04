// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CLOCK_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_CLOCK_MENU_BUTTON_H_

#include "base/timer.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/pref_member.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/view_menu_delegate.h"

class Browser;

namespace chromeos {

// The clock menu button in the status area.
// This button shows the current time.
class ClockMenuButton : public views::MenuButton,
                        public views::ViewMenuDelegate,
                        public menus::MenuModel,
                        public NotificationObserver {
 public:
  explicit ClockMenuButton(Browser* browser);
  virtual ~ClockMenuButton() {}

  // menus::MenuModel implementation.
  virtual bool HasIcons() const  { return false; }
  virtual int GetItemCount() const;
  virtual menus::MenuModel::ItemType GetTypeAt(int index) const;
  virtual int GetCommandIdAt(int index) const { return index; }
  virtual string16 GetLabelAt(int index) const;
  virtual bool IsLabelDynamicAt(int index) const { return true; }
  virtual bool GetAcceleratorAt(int index,
      menus::Accelerator* accelerator) const { return false; }
  virtual bool IsItemCheckedAt(int index) const { return false; }
  virtual int GetGroupIdAt(int index) const { return 0; }
  virtual bool GetIconAt(int index, SkBitmap* icon) const { return false; }
  virtual bool IsEnabledAt(int index) const;
  virtual menus::MenuModel* GetSubmenuModelAt(int index) const { return NULL; }
  virtual void HighlightChangedTo(int index) {}
  virtual void ActivatedAt(int index);
  virtual void MenuWillShow() {}

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Updates text and schedules the timer to fire at the next minute interval.
  void UpdateTextAndSetNextTimer();

  // Updates the time on the menu button.
  void UpdateText();

  base::OneShotTimer<ClockMenuButton> timer_;

  StringPrefMember timezone_;

  // The clock menu.
  views::Menu2 clock_menu_;

  // The browser window that owns us.
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(ClockMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CLOCK_MENU_BUTTON_H_
