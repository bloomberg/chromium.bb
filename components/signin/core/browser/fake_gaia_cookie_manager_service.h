// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_FAKE_GAIA_COOKIE_MANAGER_SERVICE_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_FAKE_GAIA_COOKIE_MANAGER_SERVICE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/list_accounts_test_utils.h"
#include "services/network/test/test_url_loader_factory.h"

namespace network {
class WeakWrapperSharedURLLoaderFactory;
}

class FakeGaiaCookieManagerService : public GaiaCookieManagerService {
 public:
  // Convenience constructor overload which uses the SharedURLLoaderFactory from
  // SigninClient.
  FakeGaiaCookieManagerService(OAuth2TokenService* token_service,
                               SigninClient* client);

  // Constructor overload for tests that want to use a TestURLLoaderFactory for
  // cookie related requests.
  FakeGaiaCookieManagerService(
      OAuth2TokenService* token_service,
      SigninClient* client,
      network::TestURLLoaderFactory* test_url_loader_factory);

  ~FakeGaiaCookieManagerService() override;

  void SetListAccountsResponseHttpNotFound();
  void SetListAccountsResponseWebLoginRequired();
  void SetListAccountsResponseWithParams(
      const std::vector<signin::CookieParams>& params);

  // Helper methods, equivalent to calling SetListAccountsResponseWithParams().
  void SetListAccountsResponseNoAccounts();
  void SetListAccountsResponseOneAccount(const std::string& email,
                                         const std::string& gaia_id);
  void SetListAccountsResponseOneAccountWithParams(
      const signin::CookieParams& params);
  void SetListAccountsResponseTwoAccounts(const std::string& email1,
                                          const std::string& gaia_id1,
                                          const std::string& email2,
                                          const std::string& gaia_id2);

 private:
  // Internal constructor which does the actual construction.
  FakeGaiaCookieManagerService(
      OAuth2TokenService* token_service,
      SigninClient* client,
      network::TestURLLoaderFactory* test_url_loader_factory,
      scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
          shared_url_loader_factory);

  // Provides a fake response for calls to /ListAccounts.
  // Owned by the client if passed in via the constructor that takes in this
  // pointer; null otherwise.
  network::TestURLLoaderFactory* test_url_loader_factory_ = nullptr;

  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      shared_url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeGaiaCookieManagerService);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_FAKE_GAIA_COOKIE_MANAGER_SERVICE_H_
