// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_SIGNIN_FAKE_GAIA_COOKIE_MANAGER_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_FAKE_GAIA_COOKIE_MANAGER_SERVICE_H_

#include "components/signin/core/browser/gaia_cookie_manager_service.h"

#include "net/url_request/test_url_fetcher_factory.h"

namespace content {
class BrowserContext;
}

class FakeGaiaCookieManagerService : public GaiaCookieManagerService {
 public:
  FakeGaiaCookieManagerService(OAuth2TokenService* token_service,
                               const std::string& source,
                               SigninClient* client);

  void Init(net::FakeURLFetcherFactory* url_fetcher_factory);

  void SetListAccountsResponseHttpNotFound();
  void SetListAccountsResponseNoAccounts();
  void SetListAccountsResponseOneAccount(const char* account);
  void SetListAccountsResponseOneAccountWithExpiry(
      const char* account, bool expired);
  void SetListAccountsResponseTwoAccounts(const char* account1,
                                          const char* account2);
  void SetListAccountsResponseTwoAccountsWithExpiry(
      const char* account1, bool account1_expired,
      const char* account2, bool account2_expired);

  // Helper function to be used with KeyedService::SetTestingFactory().
  static KeyedService* Build(content::BrowserContext* context);

 private:
  // Provide a fake response for calls to /ListAccounts.
  net::FakeURLFetcherFactory* url_fetcher_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeGaiaCookieManagerService);
};

#endif  // CHROME_BROWSER_SIGNIN_FAKE_GAIA_COOKIE_MANAGER_SERVICE_H_
