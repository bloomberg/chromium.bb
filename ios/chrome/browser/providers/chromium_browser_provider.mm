// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/providers/chromium_browser_provider.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/providers/chromium_logo_controller.h"
#import "ios/chrome/browser/providers/chromium_voice_search_provider.h"
#import "ios/chrome/browser/providers/images/chromium_branded_image_provider.h"
#include "ios/public/provider/chrome/browser/distribution/app_distribution_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "ios/public/provider/chrome/browser/signin/signin_error_provider.h"
#include "ios/public/provider/chrome/browser/signin/signin_resources_provider.h"
#include "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromiumBrowserProvider::ChromiumBrowserProvider()
    : app_distribution_provider_(base::MakeUnique<AppDistributionProvider>()),
      branded_image_provider_(base::MakeUnique<ChromiumBrandedImageProvider>()),
      signin_error_provider_(base::MakeUnique<ios::SigninErrorProvider>()),
      signin_resources_provider_(
          base::MakeUnique<ios::SigninResourcesProvider>()),
      user_feedback_provider_(base::MakeUnique<UserFeedbackProvider>()),
      voice_search_provider_(base::MakeUnique<ChromiumVoiceSearchProvider>()) {}

ChromiumBrowserProvider::~ChromiumBrowserProvider() {}

ios::SigninErrorProvider* ChromiumBrowserProvider::GetSigninErrorProvider() {
  return signin_error_provider_.get();
}

ios::SigninResourcesProvider*
ChromiumBrowserProvider::GetSigninResourcesProvider() {
  return signin_resources_provider_.get();
}

void ChromiumBrowserProvider::SetChromeIdentityServiceForTesting(
    std::unique_ptr<ios::ChromeIdentityService> service) {
  chrome_identity_service_ = std::move(service);
}

ios::ChromeIdentityService*
ChromiumBrowserProvider::GetChromeIdentityService() {
  if (!chrome_identity_service_) {
    chrome_identity_service_ = base::MakeUnique<ios::ChromeIdentityService>();
  }
  return chrome_identity_service_.get();
}

void ChromiumBrowserProvider::InitializeCastService(id main_tab_model) const {}

void ChromiumBrowserProvider::AttachTabHelpers(web::WebState* web_state,
                                               id tab) const {}

VoiceSearchProvider* ChromiumBrowserProvider::GetVoiceSearchProvider() const {
  return voice_search_provider_.get();
}

id<LogoVendor> ChromiumBrowserProvider::CreateLogoVendor(
    ios::ChromeBrowserState* browser_state,
    id<UrlLoader> loader) const {
  return [[ChromiumLogoController alloc] init];
}

UserFeedbackProvider* ChromiumBrowserProvider::GetUserFeedbackProvider() const {
  return user_feedback_provider_.get();
}

AppDistributionProvider* ChromiumBrowserProvider::GetAppDistributionProvider()
    const {
  return app_distribution_provider_.get();
}

BrandedImageProvider* ChromiumBrowserProvider::GetBrandedImageProvider() const {
  return branded_image_provider_.get();
}
