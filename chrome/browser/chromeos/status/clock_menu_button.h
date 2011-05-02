// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_CLOCK_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_CLOCK_MENU_BUTTON_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/cros/power_library.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_type.h"
#include "chrome/browser/chromeos/system_access.h"
#include "unicode/calendar.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/view_menu_delegate.h"

namespace chromeos {

class StatusAreaHost;

// The clock menu button in the status area.
// This button shows the current time.
class ClockMenuButton : public StatusAreaButton,
                        public views::ViewMenuDelegate,
                        public ui::MenuModel,
                        public NotificationObserver,
                        public PowerLibrary::Observer,
                        public SystemAccess::Observer {
 public:
  explicit ClockMenuButton(StatusAreaHost* host);
  virtual ~ClockMenuButton();

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
  virtual bool GetIconAt(int index, SkBitmap* icon) { return false; }
  virtual ui::ButtonMenuItemModel* GetButtonMenuItemAt(int index) const {
    return NULL;
  }
  virtual bool IsEnabledAt(int index) const;
  virtual ui::MenuModel* GetSubmenuModelAt(int index) const { return NULL; }
  virtual void HighlightChangedTo(int index) {}
  virtual void ActivatedAt(int index);
  virtual void MenuWillShow() {}
  virtual void SetMenuModelDelegate(ui::MenuModelDelegate* delegate) {}

  // Overridden from ResumeLibrary::Observer:
  virtual void PowerChanged(PowerLibrary* obj) {}
  virtual void SystemResumed();

  // Overridden from SystemAccess::Observer:
  virtual void TimezoneChanged(const icu::TimeZone& timezone);

  // views::View
  virtual void OnLocaleChanged() OVERRIDE;

  // Updates the time on the menu button. Can be called by host if timezone
  // changes.
  void UpdateText();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  virtual int horizontal_padding() { return 3; }

 private:
  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Updates text and schedules the timer to fire at the next minute interval.
  void UpdateTextAndSetNextTimer();

  base::OneShotTimer<ClockMenuButton> timer_;

  // The clock menu.
  // NOTE: we use a scoped_ptr here as menu calls into 'this' from the
  // constructor.
  scoped_ptr<views::Menu2> clock_menu_;

  PrefChangeRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ClockMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_CLOCK_MENU_BUTTON_H_
