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
#include "content/common/notification_observer.h"
#include "views/controls/menu/view_menu_delegate.h"

namespace chromeos {

class StatusAreaHost;

// A class for the button in the status area which alerts the user when caps
// lock is active.
class CapsLockMenuButton : public NotificationObserver,
                           public StatusAreaButton,
                           public views::ViewMenuDelegate,
                           public SystemKeyEventListener::CapsLockObserver {
 public:
  explicit CapsLockMenuButton(StatusAreaHost* host);
  virtual ~CapsLockMenuButton();

  // views::View implementation.
  virtual void OnLocaleChanged();

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* unused_source, const gfx::Point& pt);

  // SystemKeyEventListener::CapsLockObserver implementation
  virtual void OnCapsLockChange(bool enabled);

  // NotificationObserver implementation
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Updates the tooltip text and the accessible name.
  void UpdateTooltip();
  // Updates the UI from the current state.
  void UpdateUIFromCurrentCapsLock(bool enabled);

 private:
  PrefService* prefs_;
  IntegerPrefMember remap_search_key_to_;

  DISALLOW_COPY_AND_ASSIGN(CapsLockMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_CAPS_LOCK_MENU_BUTTON_H_
