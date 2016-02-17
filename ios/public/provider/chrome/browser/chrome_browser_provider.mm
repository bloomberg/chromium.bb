// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#include <cstddef>

#include "base/logging.h"
#include "components/metrics/metrics_provider.h"
#include "components/sync_sessions/synced_window_delegates_getter.h"

namespace ios {

namespace {
ChromeBrowserProvider* g_chrome_browser_provider = nullptr;
}  // namespace

void SetChromeBrowserProvider(ChromeBrowserProvider* provider) {
  g_chrome_browser_provider = provider;
}

ChromeBrowserProvider* GetChromeBrowserProvider() {
  return g_chrome_browser_provider;
}

ChromeBrowserProvider::~ChromeBrowserProvider() {}

// A dummy implementation of ChromeBrowserProvider.

ChromeBrowserProvider::ChromeBrowserProvider() {}

void ChromeBrowserProvider::AssertBrowserContextKeyedFactoriesBuilt() {}

void ChromeBrowserProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {}

ProfileOAuth2TokenServiceIOSProvider*
ChromeBrowserProvider::GetProfileOAuth2TokenServiceIOSProvider() {
  return nullptr;
}

UpdatableResourceProvider*
ChromeBrowserProvider::GetUpdatableResourceProvider() {
  return nullptr;
}

InfoBarViewPlaceholder ChromeBrowserProvider::CreateInfoBarView(
    CGRect frame,
    InfoBarViewDelegate* delegate) {
  return nullptr;
}

SigninResourcesProvider* ChromeBrowserProvider::GetSigninResourcesProvider() {
  return nullptr;
}

ChromeIdentityService* ChromeBrowserProvider::GetChromeIdentityService() {
  return nullptr;
}

LiveTabContextProvider* ChromeBrowserProvider::GetLiveTabContextProvider() {
  return nullptr;
}

GeolocationUpdaterProvider*
ChromeBrowserProvider::GetGeolocationUpdaterProvider() {
  return nullptr;
}

std::string ChromeBrowserProvider::DataReductionProxyAvailability() {
  return "default";
}

std::string ChromeBrowserProvider::GetDistributionBrandCode() {
  return std::string();
}

void ChromeBrowserProvider::SetUIViewAlphaWithAnimation(UIView* view,
                                                        float alpha) {}

autofill::CardUnmaskPromptView*
ChromeBrowserProvider::CreateCardUnmaskPromptView(
    autofill::CardUnmaskPromptController* controller) {
  return nullptr;
}

std::string ChromeBrowserProvider::GetRiskData() {
  return std::string();
}

bool ChromeBrowserProvider::IsOffTheRecordSessionActive() {
  return false;
}

void ChromeBrowserProvider::GetFaviconForURL(
    ios::ChromeBrowserState* browser_state,
    const GURL& page_url,
    const std::vector<int>& desired_sizes_in_pixel,
    const favicon_base::FaviconResultsCallback& callback) const {}

bool ChromeBrowserProvider::IsSafeBrowsingEnabled(
    const base::Closure& on_update_callback) {
  return false;
}

void ChromeBrowserProvider::OnMetricsServicesManagerClientDestroyed() {}

scoped_ptr<browser_sync::SyncedWindowDelegatesGetter>
ChromeBrowserProvider::CreateSyncedWindowDelegatesGetter(
    ios::ChromeBrowserState* browser_state) {
  return nullptr;
}

net::URLRequestContextGetter*
ChromeBrowserProvider::GetSafeBrowsingURLRequestContext() {
  return nullptr;
}

}  // namespace ios
