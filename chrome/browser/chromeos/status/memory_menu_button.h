// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_MEMORY_MENU_BUTTON_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_MEMORY_MENU_BUTTON_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/view_menu_delegate.h"

namespace base {
struct SystemMemoryInfoKB;
}

namespace views {
class MenuItemView;
class MenuRunner;
}

// Memory debugging display that lives in the status area.
class MemoryMenuButton : public StatusAreaButton,
                         public views::MenuDelegate,
                         public views::ViewMenuDelegate,
                         public content::NotificationObserver {
 public:
  explicit MemoryMenuButton(StatusAreaButton::Delegate* delegate);
  virtual ~MemoryMenuButton();

  // views::MenuDelegate implementation
  virtual string16 GetLabel(int id) const OVERRIDE;
  virtual bool IsCommandEnabled(int id) const OVERRIDE;
  virtual void ExecuteCommand(int id) OVERRIDE;

  // views::ViewMenuDelegate implementation.
  virtual void RunMenu(views::View* source, const gfx::Point& pt) OVERRIDE;

  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Updates the text on the menu button.
  void UpdateText();

 protected:
  virtual int horizontal_padding() OVERRIDE;

 private:
  // Execute command id for each renderer. Used for heap profiling.
  void SendCommandToRenderers(int id);

  // Creates and returns the menu. The caller owns the returned value.
  views::MenuItemView* CreateMenu();

  // Updates text and schedules the timer to fire at the next minute interval.
  void UpdateTextAndSetNextTimer();

  base::OneShotTimer<MemoryMenuButton> timer_;

  // Raw data from /proc/meminfo
  scoped_ptr<base::SystemMemoryInfoKB> meminfo_;

  content::NotificationRegistrar registrar_;

  // Number of renderer kills we have observed.
  int renderer_kills_;

  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(MemoryMenuButton);
};

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_MEMORY_MENU_BUTTON_H_
