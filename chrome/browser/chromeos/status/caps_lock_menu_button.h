// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_CAPS_LOCK_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_CAPS_LOCK_MENU_BUTTON_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/chromeos/system_key_event_listener.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/public/browser/notification_observer.h"
#include "views/controls/menu/menu_delegate.h"
#include "views/controls/menu/view_menu_delegate.h"

namespace views {
class MenuRunner;
}

namespace chromeos {

class StatusAreaHost;

// A class for the button in the status area which alerts the user when caps
// lock is active.
class CapsLockMenuButton : public content::NotificationObserver,
                           public StatusAreaButton,
                           public views::MenuDelegate,
                           public views::ViewMenuDelegate,
                           public SystemKeyEventListener::CapsLockObserver {
 public:
  explicit CapsLockMenuButton(StatusAreaHost* host);
  virtual ~CapsLockMenuButton();

  // views::View implementation.
  virtual void OnLocaleChanged();

  // views::MenuDelegate implementation.
  virtual string16 GetLabel(int id) const;
  virtual bool IsCommandEnabled(int id) const;

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* unused_source, const gfx::Point& pt);

  // SystemKeyEventListener::CapsLockObserver implementation
  virtual void OnCapsLockChange(bool enabled);

  // content::NotificationObserver implementation
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details);

  // Updates the accessible name.
  void UpdateAccessibleName();

  // Gets the text for the drop-down menu.
  string16 GetText() const;

  // Updates the UI from the current state.
  void UpdateUIFromCurrentCapsLock(bool enabled);

 private:
  class StatusView;

  PrefService* prefs_;
  IntegerPrefMember remap_search_key_to_;

  // The currently showing status view. NULL if menu is not being displayed.
  StatusView* status_;

  // If non-null the menu is showing.
  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(CapsLockMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_CAPS_LOCK_MENU_BUTTON_H_
