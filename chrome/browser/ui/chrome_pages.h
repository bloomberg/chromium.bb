// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROME_PAGES_H_
#define CHROME_BROWSER_UI_CHROME_PAGES_H_

#include <string>

#include "chrome/browser/ui/webui/sync_promo/sync_promo_ui.h"
#include "chrome/common/content_settings_types.h"

class Browser;
class GURL;

namespace content {
class WebContents;
}

namespace chrome {

// Sources of requests to show the help tab.
enum HelpSource {
  // Keyboard accelerators.
  HELP_SOURCE_KEYBOARD,

  // Menus (e.g. wrench menu or Chrome OS system menu).
  HELP_SOURCE_MENU,

  // WebUI (the "About" page).
  HELP_SOURCE_WEBUI,
};


void ShowBookmarkManager(Browser* browser);
void ShowBookmarkManagerForNode(Browser* browser, int64 node_id);
void ShowHistory(Browser* browser);
void ShowDownloads(Browser* browser);
void ShowExtensions(Browser* browser,
                    const std::string& extension_to_highlight);
void ShowConflicts(Browser* browser);
void ShowFeedbackPage(Browser* browser,
                      const std::string& description_template,
                      const std::string& category_tag);

void ShowHelp(Browser* browser, HelpSource source);
void ShowPolicy(Browser* browser);

// Various things that open in a settings UI.
void ShowSettings(Browser* browser);
void ShowSettingsSubPage(Browser* browser, const std::string& sub_page);
void ShowContentSettings(Browser* browser,
                         ContentSettingsType content_settings_type);
void ShowClearBrowsingDataDialog(Browser* browser);
void ShowPasswordManager(Browser* browser);
void ShowImportDialog(Browser* browser);
void ShowAboutChrome(Browser* browser);
void ShowSearchEngineSettings(Browser* browser);
// If the user is already signed in, shows the "Signin" portion of Settings,
// otherwise initiates signin.
void ShowBrowserSignin(Browser* browser, SyncPromoUI::Source source);

// Open a tab to sign into GAIA.
void ShowGaiaSignin(Browser* browser,
                    const std::string& service,
                    const GURL& continue_url);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_CHROME_PAGES_H_
