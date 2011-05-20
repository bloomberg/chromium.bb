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
#include "views/controls/menu/menu_delegate.h"
#include "views/controls/menu/view_menu_delegate.h"

namespace views {
class MenuItemView;
}

namespace chromeos {

class StatusAreaHost;

// The clock menu button in the status area.
// This button shows the current time.
class ClockMenuButton : public StatusAreaButton,
                        public views::MenuDelegate,
                        public views::ViewMenuDelegate,
                        public NotificationObserver,
                        public PowerLibrary::Observer,
                        public SystemAccess::Observer {
 public:
  explicit ClockMenuButton(StatusAreaHost* host);
  virtual ~ClockMenuButton();

  // views::MenuDelegate implementation
  virtual std::wstring GetLabel(int id) const OVERRIDE;
  virtual bool IsCommandEnabled(int id) const OVERRIDE;
  virtual void ExecuteCommand(int id) OVERRIDE;

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
  virtual int horizontal_padding();

 private:
  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // Create and initialize menu if not already present.
  void EnsureMenu();

  // Updates text and schedules the timer to fire at the next minute interval.
  void UpdateTextAndSetNextTimer();

  base::OneShotTimer<ClockMenuButton> timer_;

  // The clock menu.
  // NOTE: we use a scoped_ptr here as menu calls into 'this' from the
  // constructor.
  scoped_ptr<views::MenuItemView> menu_;

  PrefChangeRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ClockMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_CLOCK_MENU_BUTTON_H_
