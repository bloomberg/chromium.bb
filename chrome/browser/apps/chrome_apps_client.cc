// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/chrome_apps_client.h"

#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/apps/app_metro_infobar_delegate_win.h"
#include "chrome/common/extensions/extension.h"

#if defined(OS_WIN)
#include "win8/util/win8_util.h"
#endif

ChromeAppsClient::ChromeAppsClient() {}

ChromeAppsClient::~ChromeAppsClient() {}

// static
ChromeAppsClient* ChromeAppsClient::GetInstance() {
  return Singleton<ChromeAppsClient,
                   LeakySingletonTraits<ChromeAppsClient> >::get();
}

std::vector<content::BrowserContext*>
    ChromeAppsClient::GetLoadedBrowserContexts() {
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  return std::vector<content::BrowserContext*>(profiles.begin(),
                                               profiles.end());
}

bool ChromeAppsClient::CheckAppLaunch(content::BrowserContext* context,
                                      const extensions::Extension* extension) {
#if defined(OS_WIN)
  // On Windows 8's single window Metro mode we can not launch platform apps.
  // Offer to switch Chrome to desktop mode.
  if (win8::IsSingleWindowMetroMode()) {
    AppMetroInfoBarDelegateWin::Create(
        Profile::FromBrowserContext(context),
        AppMetroInfoBarDelegateWin::LAUNCH_PACKAGED_APP,
        extension->id());
    return false;
  }
#endif
  return true;
}
