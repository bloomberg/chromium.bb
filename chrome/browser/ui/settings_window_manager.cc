// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/settings_window_manager.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"

namespace chrome {

// static
SettingsWindowManager* SettingsWindowManager::GetInstance() {
  return Singleton<SettingsWindowManager>::get();
}

void SettingsWindowManager::ShowForProfile(Profile* profile,
                                           const std::string& sub_page) {
  content::RecordAction(base::UserMetricsAction("ShowOptions"));
  GURL gurl = chrome::GetSettingsUrl(sub_page);
  // Look for an existing browser window.
  ProfileSessionMap::iterator iter = settings_session_map_.find(profile);
  if (iter != settings_session_map_.end()) {
    Browser* browser = chrome::FindBrowserWithID(iter->second);
    if (browser) {
      DCHECK(browser->profile() == profile);
      const content::WebContents* web_contents =
          browser->tab_strip_model()->GetWebContentsAt(0);
      if (web_contents && web_contents->GetURL() == gurl) {
        browser->window()->Show();
        return;
      }
      NavigateParams params(browser, gurl,
                            content::PAGE_TRANSITION_AUTO_BOOKMARK);
      params.window_action = NavigateParams::SHOW_WINDOW;
      params.user_gesture = true;
      chrome::Navigate(&params);
      return;
    }
  }
  // No existing browser window, create one.
  NavigateParams params(profile, gurl, content::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = NEW_POPUP;
  params.trusted_source = true;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.user_gesture = true;
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  chrome::Navigate(&params);
  settings_session_map_[profile] = params.browser->session_id().id();
}

SettingsWindowManager::SettingsWindowManager() {
}

SettingsWindowManager::~SettingsWindowManager() {
}

}  // namespace chrome
