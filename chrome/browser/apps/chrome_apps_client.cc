// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/chrome_apps_client.h"

#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"

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
