// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/chrome_controller_client.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"

using content::Referrer;

ChromeControllerClient::ChromeControllerClient(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

ChromeControllerClient::~ChromeControllerClient() {}

void ChromeControllerClient::OpenUrlInCurrentTab(const GURL& url) {
  content::OpenURLParams params(url, Referrer(), CURRENT_TAB,
                                ui::PAGE_TRANSITION_LINK, false);
  web_contents_->OpenURL(params);
}

const std::string& ChromeControllerClient::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

PrefService* ChromeControllerClient::GetPrefService() {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  return profile->GetPrefs();
}

const std::string ChromeControllerClient::GetExtendedReportingPrefName() {
  return prefs::kSafeBrowsingExtendedReportingEnabled;
}
