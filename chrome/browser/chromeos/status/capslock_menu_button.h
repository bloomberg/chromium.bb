// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_CAPSLOCK_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_CAPSLOCK_MENU_BUTTON_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/chromeos/system_key_event_listener.h"
#include "views/controls/menu/view_menu_delegate.h"

namespace chromeos {

class StatusAreaHost;

// A class for the button in the status area which alerts the user when caps
// lock is active.
class CapslockMenuButton : public StatusAreaButton,
                           public views::ViewMenuDelegate,
                           public SystemKeyEventListener::CapslockObserver {
 public:
  explicit CapslockMenuButton(StatusAreaHost* host);
  virtual ~CapslockMenuButton();

  // views::View implementation.
  virtual gfx::Size GetPreferredSize();
  virtual void OnLocaleChanged();

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* unused_source, const gfx::Point& pt);

  // SystemKeyEventListener::CapslockObserver implementation
  virtual void OnCapslockChange();

  // Updates the UI from the current state.
  void UpdateUIFromCurrentCapslock();

 private:
  DISALLOW_COPY_AND_ASSIGN(CapslockMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_CAPSLOCK_MENU_BUTTON_H_
