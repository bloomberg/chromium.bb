// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_FAKE_H_
#define IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_FAKE_H_

#include <memory>

#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"

namespace web {
class BrowserState;
}

class AuthenticationServiceFake : public AuthenticationService {
 public:
  static std::unique_ptr<KeyedService> CreateAuthenticationService(
      web::BrowserState* browser_state);

  ~AuthenticationServiceFake() override;

  void SignIn(ChromeIdentity* identity,
              const std::string& hosted_domain) override;

  void SignOut(signin_metrics::ProfileSignout signout_source,
               ProceduralBlock completion) override;

  void SetHaveAccountsChanged(bool changed);

  bool HaveAccountsChanged() override;

  bool IsAuthenticated() override;

  ChromeIdentity* GetAuthenticatedIdentity() override;

  NSString* GetAuthenticatedUserEmail() override;

 private:
  explicit AuthenticationServiceFake(ios::ChromeBrowserState* browser_state);

  base::scoped_nsobject<ChromeIdentity> authenticated_identity_;
  bool have_accounts_changed_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_AUTHENTICATION_SERVICE_FAKE_H_
