// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "ios/public/provider/chrome/browser/distribution/test_app_distribution_provider.h"
#include "ios/public/provider/chrome/browser/images/test_branded_image_provider.h"
#include "ios/public/provider/chrome/browser/omaha/test_omaha_service_provider.h"
#include "ios/public/provider/chrome/browser/sessions/test_live_tab_context_provider.h"
#include "ios/public/provider/chrome/browser/sessions/test_synced_window_delegates_getter.h"
#include "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#include "ios/public/provider/chrome/browser/signin/test_signin_resources_provider.h"
#import "ios/public/provider/chrome/browser/ui/test_infobar_view.h"
#import "ios/public/provider/chrome/browser/ui/test_styled_text_field.h"
#import "ios/public/provider/chrome/browser/user_feedback/test_user_feedback_provider.h"
#import "ios/public/provider/chrome/browser/voice/test_voice_search_provider.h"
#import "ios/public/provider/chrome/browser/voice/voice_search_language.h"

namespace ios {

TestChromeBrowserProvider::TestChromeBrowserProvider()
    : app_distribution_provider_(
          base::MakeUnique<TestAppDistributionProvider>()),
      branded_image_provider_(base::MakeUnique<TestBrandedImageProvider>()),
      live_tab_context_provider_(
          base::MakeUnique<TestLiveTabContextProvider>()),
      omaha_service_provider_(base::MakeUnique<TestOmahaServiceProvider>()),
      signin_resources_provider_(
          base::MakeUnique<TestSigninResourcesProvider>()),
      voice_search_provider_(base::MakeUnique<TestVoiceSearchProvider>()),
      user_feedback_provider_(base::MakeUnique<TestUserFeedbackProvider>()) {}

TestChromeBrowserProvider::~TestChromeBrowserProvider() {}

// static
TestChromeBrowserProvider* TestChromeBrowserProvider::GetTestProvider() {
  ChromeBrowserProvider* provider = GetChromeBrowserProvider();
  DCHECK(provider);
  return static_cast<TestChromeBrowserProvider*>(provider);
}

InfoBarViewPlaceholder TestChromeBrowserProvider::CreateInfoBarView(
    CGRect frame,
    InfoBarViewDelegate* delegate) {
  return [[TestInfoBarView alloc] init];
}

SigninResourcesProvider*
TestChromeBrowserProvider::GetSigninResourcesProvider() {
  return signin_resources_provider_.get();
}

void TestChromeBrowserProvider::SetChromeIdentityServiceForTesting(
    std::unique_ptr<ChromeIdentityService> service) {
  chrome_identity_service_.swap(service);
}

ChromeIdentityService* TestChromeBrowserProvider::GetChromeIdentityService() {
  if (!chrome_identity_service_) {
    chrome_identity_service_.reset(new FakeChromeIdentityService());
  }
  return chrome_identity_service_.get();
}

LiveTabContextProvider* TestChromeBrowserProvider::GetLiveTabContextProvider() {
  return live_tab_context_provider_.get();
}

UITextField<TextFieldStyling>* TestChromeBrowserProvider::CreateStyledTextField(
    CGRect frame) const {
  return [[TestStyledTextField alloc] initWithFrame:frame];
}

VoiceSearchProvider* TestChromeBrowserProvider::GetVoiceSearchProvider() const {
  return voice_search_provider_.get();
}

AppDistributionProvider* TestChromeBrowserProvider::GetAppDistributionProvider()
    const {
  return app_distribution_provider_.get();
}

OmahaServiceProvider* TestChromeBrowserProvider::GetOmahaServiceProvider()
    const {
  return omaha_service_provider_.get();
}

UserFeedbackProvider* TestChromeBrowserProvider::GetUserFeedbackProvider()
    const {
  return user_feedback_provider_.get();
}

std::unique_ptr<sync_sessions::SyncedWindowDelegatesGetter>
TestChromeBrowserProvider::CreateSyncedWindowDelegatesGetter(
    ios::ChromeBrowserState* browser_state) {
  return base::MakeUnique<TestSyncedWindowDelegatesGetter>();
}

BrandedImageProvider* TestChromeBrowserProvider::GetBrandedImageProvider()
    const {
  return branded_image_provider_.get();
}

id<NativeAppWhitelistManager>
TestChromeBrowserProvider::GetNativeAppWhitelistManager() const {
  return nil;
}

}  // namespace ios
