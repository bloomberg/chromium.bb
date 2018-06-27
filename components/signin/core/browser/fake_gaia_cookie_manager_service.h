// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_FAKE_GAIA_COOKIE_MANAGER_SERVICE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_FAKE_GAIA_COOKIE_MANAGER_SERVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "net/url_request/test_url_fetcher_factory.h"

class FakeGaiaCookieManagerService : public GaiaCookieManagerService {
 public:
  // Parameters for the fake ListAccounts response.
  struct CookieParams {
    std::string email;
    std::string gaia_id;
    bool valid;
    bool signed_out;
    bool verified;
  };

  FakeGaiaCookieManagerService(OAuth2TokenService* token_service,
                               const std::string& source,
                               SigninClient* client);

  void Init(net::FakeURLFetcherFactory* url_fetcher_factory);

  void SetListAccountsResponseHttpNotFound();
  void SetListAccountsResponseWebLoginRequired();
  void SetListAccountsResponseWithParams(
      const std::vector<CookieParams>& params);

  // Helper methods, equivalent to calling SetListAccountsResponseWithParams().
  void SetListAccountsResponseNoAccounts();
  void SetListAccountsResponseOneAccount(const std::string& email,
                                         const std::string& gaia_id);
  void SetListAccountsResponseOneAccountWithParams(const CookieParams& params);
  void SetListAccountsResponseTwoAccounts(const std::string& email1,
                                          const std::string& gaia_id1,
                                          const std::string& email2,
                                          const std::string& gaia_id2);

 private:
  std::string GetSourceForRequest(
      const GaiaCookieManagerService::GaiaCookieRequest& request) override;
  std::string GetDefaultSourceForRequest() override;

  // Provide a fake response for calls to /ListAccounts.
  net::FakeURLFetcherFactory* url_fetcher_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeGaiaCookieManagerService);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_FAKE_GAIA_COOKIE_MANAGER_SERVICE_H_
