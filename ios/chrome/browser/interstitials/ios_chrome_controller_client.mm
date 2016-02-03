// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/interstitials/ios_chrome_controller_client.h"

#include "base/logging.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/web/public/interstitials/web_interstitial.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"

IOSChromeControllerClient::IOSChromeControllerClient(web::WebState* web_state)
    : web_state_(web_state), web_interstitial_(nullptr) {}

IOSChromeControllerClient::~IOSChromeControllerClient() {}

void IOSChromeControllerClient::SetWebInterstitial(
    web::WebInterstitial* web_interstitial) {
  web_interstitial_ = web_interstitial;
}

bool IOSChromeControllerClient::CanLaunchDateAndTimeSettings() {
  return false;
}

void IOSChromeControllerClient::LaunchDateAndTimeSettings() {
  NOTREACHED();
}

void IOSChromeControllerClient::GoBack() {
  DCHECK(web_interstitial_);
  web_interstitial_->DontProceed();
}

void IOSChromeControllerClient::Proceed() {
  DCHECK(web_interstitial_);
  web_interstitial_->Proceed();
}

void IOSChromeControllerClient::Reload() {
  web_state_->GetNavigationManager()->Reload(true);
}

void IOSChromeControllerClient::OpenUrlInCurrentTab(const GURL& url) {
  web_state_->OpenURL(web::WebState::OpenURLParams(
      url, web::Referrer(), CURRENT_TAB, ui::PAGE_TRANSITION_LINK, false));
}

const std::string& IOSChromeControllerClient::GetApplicationLocale() {
  return GetApplicationContext()->GetApplicationLocale();
}

PrefService* IOSChromeControllerClient::GetPrefService() {
  return ios::ChromeBrowserState::FromBrowserState(
             web_state_->GetBrowserState())
      ->GetPrefs();
}

const std::string IOSChromeControllerClient::GetExtendedReportingPrefName() {
  return prefs::kSafeBrowsingExtendedReportingEnabled;
}
