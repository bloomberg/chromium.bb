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

void ChromeBrowserProvider::Initialize() const {}

void ChromeBrowserProvider::AssertBrowserContextKeyedFactoriesBuilt() {}

void ChromeBrowserProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {}

InfoBarViewPlaceholder ChromeBrowserProvider::CreateInfoBarView(
    CGRect frame,
    InfoBarViewDelegate* delegate) {
  return nullptr;
}

SigninErrorProvider* ChromeBrowserProvider::GetSigninErrorProvider() {
  return nullptr;
}

SigninResourcesProvider* ChromeBrowserProvider::GetSigninResourcesProvider() {
  return nullptr;
}

void ChromeBrowserProvider::SetChromeIdentityServiceForTesting(
    std::unique_ptr<ChromeIdentityService> service) {}

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

UITextField<TextFieldStyling>* ChromeBrowserProvider::CreateStyledTextField(
    CGRect frame) const {
  return nil;
}

id<AppRatingPrompt> ChromeBrowserProvider::CreateAppRatingPrompt() const {
  return nil;
}

void ChromeBrowserProvider::InitializeCastService(id main_tab_model) const {}

void ChromeBrowserProvider::AttachTabHelpers(web::WebState* web_state,
                                             id tab) const {}

bool ChromeBrowserProvider::IsSafeBrowsingEnabled(
    const base::Closure& on_update_callback) {
  return false;
}

std::unique_ptr<sync_sessions::SyncedWindowDelegatesGetter>
ChromeBrowserProvider::CreateSyncedWindowDelegatesGetter(
    ios::ChromeBrowserState* browser_state) {
  return nullptr;
}

VoiceSearchProvider* ChromeBrowserProvider::GetVoiceSearchProvider() const {
  return nullptr;
}

AppDistributionProvider* ChromeBrowserProvider::GetAppDistributionProvider()
    const {
  return nullptr;
}

id<LogoVendor> ChromeBrowserProvider::CreateLogoVendor(
    ios::ChromeBrowserState* browser_state,
    id<UrlLoader> loader) const {
  return nil;
}

OmahaServiceProvider* ChromeBrowserProvider::GetOmahaServiceProvider() const {
  return nullptr;
}

UserFeedbackProvider* ChromeBrowserProvider::GetUserFeedbackProvider() const {
  return nullptr;
}

BrandedImageProvider* ChromeBrowserProvider::GetBrandedImageProvider() const {
  return nullptr;
}

id<NativeAppWhitelistManager>
ChromeBrowserProvider::GetNativeAppWhitelistManager() const {
  return nil;
}

void ChromeBrowserProvider::HideModalViewStack() const {}

void ChromeBrowserProvider::LogIfModalViewsArePresented() const {}

}  // namespace ios
