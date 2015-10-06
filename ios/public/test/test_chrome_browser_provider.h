// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_TEST_TEST_CHROME_BROWSER_PROVIDER_H_
#define IOS_PUBLIC_TEST_TEST_CHROME_BROWSER_PROVIDER_H_

#include "base/memory/scoped_ptr.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

namespace ios {

class FakeProfileOAuth2TokenServiceIOSProvider;
class FakeStringProvider;

class TestChromeBrowserProvider : public ChromeBrowserProvider {
 public:
  TestChromeBrowserProvider();
  ~TestChromeBrowserProvider() override;

  // Returns the current provider as a |TestChromeBrowserProvider|.
  static TestChromeBrowserProvider* GetTestProvider();

  // ChromeBrowserProvider:
  ChromeIdentityService* GetChromeIdentityService() override;
  StringProvider* GetStringProvider() override;

  // Returns the string provider as a |FakeStringProvider|.
  FakeStringProvider* GetStringProviderAsFake();

 private:
  scoped_ptr<ChromeIdentityService> chrome_identity_service_;
  scoped_ptr<FakeStringProvider> string_provider_;

  DISALLOW_COPY_AND_ASSIGN(TestChromeBrowserProvider);
};

}  // namespace ios

#endif  // IOS_PUBLIC_TEST_TEST_CHROME_BROWSER_PROVIDER_H_
