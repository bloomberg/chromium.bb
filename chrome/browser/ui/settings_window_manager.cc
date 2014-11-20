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
#include "chrome/browser/ui/settings_window_manager_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace chrome {

// static
SettingsWindowManager* SettingsWindowManager::GetInstance() {
  return Singleton<SettingsWindowManager>::get();
}

void SettingsWindowManager::AddObserver(
    SettingsWindowManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void SettingsWindowManager::RemoveObserver(
    SettingsWindowManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SettingsWindowManager::ShowChromePageForProfile(Profile* profile,
                                                     const GURL& gurl) {
  // Use the original (non off-the-record) profile for settings unless
  // this is a guest session.
  if (!profile->IsGuestSession() && profile->IsOffTheRecord())
    profile = profile->GetOriginalProfile();

  // Look for an existing browser window.
  Browser* browser = FindBrowserForProfile(profile);
  if (browser) {
    DCHECK(browser->profile() == profile);
    const content::WebContents* web_contents =
        browser->tab_strip_model()->GetWebContentsAt(0);
    if (web_contents && web_contents->GetURL() == gurl) {
      browser->window()->Show();
      return;
    }
    NavigateParams params(browser, gurl,
                          ui::PAGE_TRANSITION_AUTO_BOOKMARK);
    params.window_action = NavigateParams::SHOW_WINDOW;
    params.user_gesture = true;
    chrome::Navigate(&params);
    return;
  }

  // No existing browser window, create one.
  NavigateParams params(profile, gurl, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = NEW_POPUP;
  params.trusted_source = true;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.user_gesture = true;
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  chrome::Navigate(&params);
  settings_session_map_[profile] = params.browser->session_id().id();
  DCHECK(params.browser->is_trusted_source());

  FOR_EACH_OBSERVER(SettingsWindowManagerObserver,
                    observers_, OnNewSettingsWindow(params.browser));
}

Browser* SettingsWindowManager::FindBrowserForProfile(Profile* profile) {
  ProfileSessionMap::iterator iter = settings_session_map_.find(profile);
  if (iter != settings_session_map_.end())
    return chrome::FindBrowserWithID(iter->second);
  return NULL;
}

bool SettingsWindowManager::IsSettingsBrowser(Browser* browser) const {
  ProfileSessionMap::const_iterator iter =
      settings_session_map_.find(browser->profile());
  return (iter != settings_session_map_.end() &&
          iter->second == browser->session_id().id());
}

SettingsWindowManager::SettingsWindowManager() {
}

SettingsWindowManager::~SettingsWindowManager() {
}

}  // namespace chrome
