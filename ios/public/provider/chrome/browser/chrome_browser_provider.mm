// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#include <cstddef>

#include "base/logging.h"
#include "components/metrics/metrics_provider.h"

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

// A dummy implementation of ChromeBrowserProvider.

ChromeBrowserProvider::ChromeBrowserProvider() {}

ChromeBrowserProvider::~ChromeBrowserProvider() {}

void ChromeBrowserProvider::Initialize() const {}

void ChromeBrowserProvider::AssertBrowserContextKeyedFactoriesBuilt() {}

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

GeolocationUpdaterProvider*
ChromeBrowserProvider::GetGeolocationUpdaterProvider() {
  return nullptr;
}

std::string ChromeBrowserProvider::GetRiskData() {
  return std::string();
}

UITextField<TextFieldStyling>* ChromeBrowserProvider::CreateStyledTextField(
    CGRect frame) const {
  return nil;
}

id<AppRatingPrompt> ChromeBrowserProvider::CreateAppRatingPrompt() const {
  return nil;
}

void ChromeBrowserProvider::InitializeCastService(
    TabModel* main_tab_model) const {}

void ChromeBrowserProvider::AttachTabHelpers(web::WebState* web_state,
                                             Tab* tab) const {}

bool ChromeBrowserProvider::IsSafeBrowsingEnabled(
    const base::Closure& on_update_callback) {
  return false;
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

SpotlightProvider* ChromeBrowserProvider::GetSpotlightProvider() const {
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
