// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/test/test_chrome_browser_provider.h"

#include "base/logging.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "ios/public/test/fake_string_provider.h"

namespace ios {

TestChromeBrowserProvider::TestChromeBrowserProvider()
    : chrome_identity_service_(new ios::ChromeIdentityService),
      string_provider_(new FakeStringProvider) {}

TestChromeBrowserProvider::~TestChromeBrowserProvider() {
}

// static
TestChromeBrowserProvider* TestChromeBrowserProvider::GetTestProvider() {
  ChromeBrowserProvider* provider = GetChromeBrowserProvider();
  DCHECK(provider);
  return static_cast<TestChromeBrowserProvider*>(provider);
}

ChromeIdentityService* TestChromeBrowserProvider::GetChromeIdentityService() {
  return chrome_identity_service_.get();
}

StringProvider* TestChromeBrowserProvider::GetStringProvider() {
  return string_provider_.get();
}

FakeStringProvider*
TestChromeBrowserProvider::GetStringProviderAsFake() {
  return string_provider_.get();
}

}  // namespace ios
