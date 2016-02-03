// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_CHROME_BROWSER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_CHROME_BROWSER_PROVIDER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

class FakeProfileOAuth2TokenServiceIOSProvider;

namespace ios {

class TestUpdatableResourceProvider;

class TestChromeBrowserProvider : public ChromeBrowserProvider {
 public:
  TestChromeBrowserProvider();
  ~TestChromeBrowserProvider() override;

  // Returns the current provider as a |TestChromeBrowserProvider|.
  static TestChromeBrowserProvider* GetTestProvider();

  // ChromeBrowserProvider:
  ProfileOAuth2TokenServiceIOSProvider*
  GetProfileOAuth2TokenServiceIOSProvider() override;
  ChromeIdentityService* GetChromeIdentityService() override;
  UpdatableResourceProvider* GetUpdatableResourceProvider() override;

 private:
  scoped_ptr<FakeProfileOAuth2TokenServiceIOSProvider>
      oauth2_token_service_provider_;
  scoped_ptr<ChromeIdentityService> chrome_identity_service_;
  scoped_ptr<TestUpdatableResourceProvider> test_updatable_resource_provider_;

  DISALLOW_COPY_AND_ASSIGN(TestChromeBrowserProvider);
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_TEST_CHROME_BROWSER_PROVIDER_H_
