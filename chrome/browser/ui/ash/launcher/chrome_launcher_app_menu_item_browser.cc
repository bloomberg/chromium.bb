// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_browser.h"

#include "ash/wm/window_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_per_app.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

ChromeLauncherAppMenuItemBrowser::ChromeLauncherAppMenuItemBrowser(
    const string16 title,
    const gfx::Image* icon,
    Browser* browser)
    : ChromeLauncherAppMenuItem(title, icon),
      browser_(browser) {
  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_CLOSING,
                 content::Source<Browser>(browser));
}

bool ChromeLauncherAppMenuItemBrowser::IsActive() const {
  return browser_ == chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow());
}

bool ChromeLauncherAppMenuItemBrowser::IsEnabled() const {
  return true;
}

void ChromeLauncherAppMenuItemBrowser::Execute() {
  if (browser_) {
    browser_->window()->Show();
    ash::wm::ActivateWindow(browser_->window()->GetNativeWindow());
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
