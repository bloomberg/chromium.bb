// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/public/provider/chrome/browser/test_updatable_resource_provider.h"

namespace ios {

TestChromeBrowserProvider::TestChromeBrowserProvider()
    : test_updatable_resource_provider_(new TestUpdatableResourceProvider) {}

TestChromeBrowserProvider::~TestChromeBrowserProvider() {}

// static
TestChromeBrowserProvider* TestChromeBrowserProvider::GetTestProvider() {
  ChromeBrowserProvider* provider = GetChromeBrowserProvider();
  DCHECK(provider);
  return static_cast<TestChromeBrowserProvider*>(provider);
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

UpdatableResourceProvider*
TestChromeBrowserProvider::GetUpdatableResourceProvider() {
  return test_updatable_resource_provider_.get();
}

}  // namespace ios
