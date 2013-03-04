// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_tab.h"

#include "ash/wm/window_util.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller_per_app.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

ChromeLauncherAppMenuItemTab::ChromeLauncherAppMenuItemTab(
    const string16 title,
    const gfx::Image* icon,
    content::WebContents* content)
    : ChromeLauncherAppMenuItem(title, icon),
      content::WebContentsObserver(content) {
}

bool ChromeLauncherAppMenuItemTab::IsActive() const {
  Browser* browser = chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow());
  if (!browser)
    return false;
  return web_contents() == browser->tab_strip_model()->GetActiveWebContents();
}

bool ChromeLauncherAppMenuItemTab::IsEnabled() const {
  return true;
}

void ChromeLauncherAppMenuItemTab::Execute() {
  if (!web_contents())
    return;
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  if (!browser)
    return;
  TabStripModel* tab_strip = browser->tab_strip_model();
  int index = tab_strip->GetIndexOfWebContents(web_contents());
  DCHECK(index != TabStripModel::kNoTab);
  tab_strip->ActivateTabAt(index, false);
  browser->window()->Show();
  // Need this check to prevent unit tests from crashing.
  if (browser->window()->GetNativeWindow())
    ash::wm::ActivateWindow(browser->window()->GetNativeWindow());
}
