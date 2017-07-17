// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#include <cstddef>

#include "base/logging.h"
#include "components/metrics/metrics_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

ChromeBrowserProvider::~ChromeBrowserProvider() {
  for (auto& observer : observer_list_)
    observer.OnChromeBrowserProviderWillBeDestroyed();
}

void ChromeBrowserProvider::AppendSwitchesFromExperimentalSettings(
    NSUserDefaults* experimental_settings,
    base::CommandLine* command_line) const {}

void ChromeBrowserProvider::Initialize() const {}

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

void ChromeBrowserProvider::CheckForFirstPartyApps() const {}

BrandedImageProvider* ChromeBrowserProvider::GetBrandedImageProvider() const {
  return nullptr;
}

void ChromeBrowserProvider::HideModalViewStack() const {}

void ChromeBrowserProvider::LogIfModalViewsArePresented() const {}

void ChromeBrowserProvider::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ChromeBrowserProvider::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ChromeBrowserProvider::FireChromeIdentityServiceDidChange(
    ChromeIdentityService* new_service) {
  for (auto& observer : observer_list_)
    observer.OnChromeIdentityServiceDidChange(new_service);
}

}  // namespace ios
