// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"

#include "base/logging.h"
#include "components/signin/ios/browser/fake_profile_oauth2_token_service_ios_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "ios/public/provider/chrome/browser/test_updatable_resource_provider.h"

namespace ios {

TestChromeBrowserProvider::TestChromeBrowserProvider()
    : oauth2_token_service_provider_(
          new FakeProfileOAuth2TokenServiceIOSProvider),
      chrome_identity_service_(new ios::ChromeIdentityService),
      test_updatable_resource_provider_(new TestUpdatableResourceProvider) {}

TestChromeBrowserProvider::~TestChromeBrowserProvider() {}

// static
TestChromeBrowserProvider* TestChromeBrowserProvider::GetTestProvider() {
  ChromeBrowserProvider* provider = GetChromeBrowserProvider();
  DCHECK(provider);
  return static_cast<TestChromeBrowserProvider*>(provider);
}

ProfileOAuth2TokenServiceIOSProvider*
TestChromeBrowserProvider::GetProfileOAuth2TokenServiceIOSProvider() {
  return oauth2_token_service_provider_.get();
}

ChromeIdentityService* TestChromeBrowserProvider::GetChromeIdentityService() {
  return chrome_identity_service_.get();
}

UpdatableResourceProvider*
TestChromeBrowserProvider::GetUpdatableResourceProvider() {
  return test_updatable_resource_provider_.get();
}

}  // namespace ios
