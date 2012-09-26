// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/wallpaper_manager_util.h"

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/screen.h"

namespace wallpaper_manager_util {
namespace {

Browser* GetBrowserForUrl(GURL target_url) {
  for (BrowserList::const_iterator browser_iterator = BrowserList::begin();
       browser_iterator != BrowserList::end(); ++browser_iterator) {
    Browser* browser = *browser_iterator;
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int idx = 0; idx < tab_strip->count(); idx++) {
      content::WebContents* web_contents =
          tab_strip->GetTabContentsAt(idx)->web_contents();
      const GURL& url = web_contents->GetURL();
      if (url == target_url)
        return browser;
    }
  }
  return NULL;
}

}  // namespace

void OpenWallpaperManager() {
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  // Hides the new UI container behind a flag.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableNewWallpaperUI)) {
    std::string url = chrome::kChromeUIWallpaperURL;
    ExtensionService* service = profile->GetExtensionService();
    if (!service)
      return;

    const extensions::Extension* extension =
        service->GetExtensionById(extension_misc::kWallpaperManagerId, false);
    if (!extension)
      return;

    GURL wallpaper_picker_url(url);
    int width = extension->launch_width();
    int height = extension->launch_height();
    const gfx::Size screen = gfx::Screen::GetPrimaryDisplay().size();
    const gfx::Rect bounds((screen.width() - width) / 2,
                           (screen.height() - height) / 2,
                           width,
                           height);

    Browser* browser = GetBrowserForUrl(wallpaper_picker_url);

    if (!browser) {
      browser = new Browser(
          Browser::CreateParams::CreateForApp(Browser::TYPE_POPUP,
                                              extension->name(),
                                              bounds,
                                              profile));

      chrome::AddSelectedTabWithURL(browser, wallpaper_picker_url,
                                    content::PAGE_TRANSITION_LINK);
    }
    browser->window()->Show();
  } else {
    Browser* browser = browser::FindOrCreateTabbedBrowser(
        ProfileManager::GetDefaultProfileOrOffTheRecord());
    chrome::ShowSettingsSubPage(browser, "setWallpaper");
  }
}

}  // namespace wallpaper_manager_util
