// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_browser.h"

#include "ash/wm/window_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/notification_service.h"
#include "ui/events/event_constants.h"

ChromeLauncherAppMenuItemBrowser::ChromeLauncherAppMenuItemBrowser(
    const base::string16 title,
    const gfx::Image* icon,
    Browser* browser,
    bool has_leading_separator)
    : ChromeLauncherAppMenuItem(title, icon, has_leading_separator),
      browser_(browser) {
  DCHECK(browser);
  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_CLOSING,
                 content::Source<Browser>(browser));
}
ChromeLauncherAppMenuItemBrowser::~ChromeLauncherAppMenuItemBrowser() {
}

bool ChromeLauncherAppMenuItemBrowser::IsActive() const {
  return browser_ == chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow());
}

bool ChromeLauncherAppMenuItemBrowser::IsEnabled() const {
  return true;
}

void ChromeLauncherAppMenuItemBrowser::Execute(int event_flags) {
  if (browser_) {
    if (event_flags & (ui::EF_SHIFT_DOWN | ui::EF_MIDDLE_MOUSE_BUTTON)) {
      TabStripModel* tab_strip = browser_->tab_strip_model();
      tab_strip->CloseAllTabs();
    } else {
      browser_->window()->Show();
      ash::wm::ActivateWindow(browser_->window()->GetNativeWindow());
    }
  }
}

void ChromeLauncherAppMenuItemBrowser::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_BROWSER_CLOSING:
      DCHECK_EQ(browser_, content::Source<Browser>(source).ptr());
      browser_ = NULL;
      break;

    default:
      NOTREACHED();
  }
}
