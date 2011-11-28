// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_CLOCK_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_CLOCK_MENU_BUTTON_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_types.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/view_menu_delegate.h"
#include "unicode/calendar.h"

namespace views {
class MenuRunner;
}

// The clock menu button in the status area.
// This button shows the current time.
class ClockMenuButton : public StatusAreaButton,
                        public views::MenuDelegate,
                        public views::ViewMenuDelegate,
                        public content::NotificationObserver {
 public:
  explicit ClockMenuButton(StatusAreaButton::Delegate* delegate);
  virtual ~ClockMenuButton();

  // views::MenuDelegate implementation
  virtual string16 GetLabel(int id) const OVERRIDE;
  virtual bool IsCommandEnabled(int id) const OVERRIDE;
  virtual void ExecuteCommand(int id) OVERRIDE;

  // views::View
  virtual void OnLocaleChanged() OVERRIDE;

  // Updates the time on the menu button.
  void UpdateText();

  // Sets default use 24hour clock mode.
  void SetDefaultUse24HourClock(bool use_24hour_clock);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 protected:
  virtual int horizontal_padding() OVERRIDE;

 private:
  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt) OVERRIDE;

  // Create and initialize menu if not already present.
  void EnsureMenu();

  // Updates text and schedules the timer to fire at the next minute interval.
  void UpdateTextAndSetNextTimer();

  base::OneShotTimer<ClockMenuButton> timer_;

  // The clock menu.
  scoped_ptr<views::MenuRunner> menu_runner_;

  PrefChangeRegistrar registrar_;

  // Default value for use_24hour_clock.
  bool default_use_24hour_clock_;

  DISALLOW_COPY_AND_ASSIGN(ClockMenuButton);
};

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_CLOCK_MENU_BUTTON_H_
