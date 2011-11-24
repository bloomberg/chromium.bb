// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_ACCESSIBILITY_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_ACCESSIBILITY_MENU_BUTTON_H_
#pragma once

#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/public/browser/notification_observer.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/controls/menu/menu_delegate.h"

namespace views {
class MenuRunner;
}

namespace chromeos {

class StatusAreaBubbleController;

// A class for the button in the status area which alerts the user when
// accessibility features are enabled.
class AccessibilityMenuButton : public StatusAreaButton,
                                public views::ViewMenuDelegate,
                                public views::MenuDelegate,
                                public content::NotificationObserver {
 public:
  explicit AccessibilityMenuButton(StatusAreaButton::Delegate* delegate);
  virtual ~AccessibilityMenuButton();

  // views::ViewMenuDelegate implementation
  virtual void RunMenu(views::View* source, const gfx::Point& pt) OVERRIDE;

  // views::MenuDelegate implementation
  virtual void ExecuteCommand(int id) OVERRIDE;

  // content::NotificationObserver implementation
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Updates the state along with the preferences.
  void Update();

  // Prepares menu before showing it.
  void PrepareMenu();

  // An object synced to the preference, representing if accessibility feature
  // is enabled or not.
  BooleanPrefMember accessibility_enabled_;
  // An object to show menu.
  scoped_ptr<views::MenuRunner> menu_runner_;
  // The currently showing bubble controller.
  scoped_ptr<StatusAreaBubbleController> bubble_controller_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityMenuButton);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_ACCESSIBILITY_MENU_BUTTON_H_
