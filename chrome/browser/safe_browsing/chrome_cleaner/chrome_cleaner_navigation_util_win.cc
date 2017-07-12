// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_navigation_util_win.h"

#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace chrome_cleaner_util {

Browser* FindBrowser() {
  BrowserList* browser_list = BrowserList::GetInstance();
  for (BrowserList::const_reverse_iterator browser_iterator =
           browser_list->begin_last_active();
       browser_iterator != browser_list->end_last_active();
       ++browser_iterator) {
    Browser* browser = *browser_iterator;
    if (browser->is_type_tabbed() &&
        (browser->window()->IsActive() || !browser->window()->IsMinimized()))
      return browser;
  }

  return nullptr;
}

void OpenSettingsPage(Browser* browser,
                      WindowOpenDisposition disposition,
                      bool skip_if_current_tab) {
  DCHECK(browser);

  // Skip opening the settings page if it's already the currently active tab.
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (skip_if_current_tab && web_contents &&
      web_contents->GetLastCommittedURL() == chrome::kChromeUISettingsURL) {
    return;
  }

  browser->OpenURL(content::OpenURLParams(
      GURL(chrome::kChromeUISettingsURL), content::Referrer(), disposition,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, /*is_renderer_initiated=*/false));
}

}  // namespace chrome_cleaner_util
