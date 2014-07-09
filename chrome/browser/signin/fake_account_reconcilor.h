// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_SIGNIN_FAKE_ACCOUNT_RECONCILOR_H_
#define CHROME_BROWSER_SIGNIN_FAKE_ACCOUNT_RECONCILOR_H_

#include "components/signin/core/browser/account_reconcilor.h"

namespace content {
class BrowserContext;
}

class FakeAccountReconcilor : public AccountReconcilor {
 public:
  FakeAccountReconcilor(ProfileOAuth2TokenService* token_service,
                        SigninManagerBase* signin_manager,
                        SigninClient* client);

  // Helper function to be used with KeyedService::SetTestingFactory().
  static KeyedService* Build(content::BrowserContext* context);

 protected:
  // Override this method to perform no network call, instead the callback
  // is called immediately
  virtual void GetAccountsFromCookie(GetAccountsFromCookieCallback callback)
      OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(FakeAccountReconcilor);
};

#endif  // CHROME_BROWSER_SIGNIN_FAKE_ACCOUNT_RECONCILOR_H_
