// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/signin/core/browser/list_accounts_test_utils.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace {
// Factory method to return a SharedURLLoaderFactory of our choosing.
scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory) {
  return shared_url_loader_factory;
}
}  // namespace

FakeGaiaCookieManagerService::FakeGaiaCookieManagerService(
    OAuth2TokenService* token_service,
    SigninClient* client)
    : GaiaCookieManagerService(
          token_service,
          client,
          base::BindRepeating(&SigninClient::GetURLLoaderFactory,
                              base::Unretained(client))) {}

FakeGaiaCookieManagerService::FakeGaiaCookieManagerService(
    OAuth2TokenService* token_service,
    SigninClient* client,
    network::TestURLLoaderFactory* test_url_loader_factory)
    : FakeGaiaCookieManagerService(
          token_service,
          client,
          test_url_loader_factory,
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              test_url_loader_factory)) {}

FakeGaiaCookieManagerService::FakeGaiaCookieManagerService(
    OAuth2TokenService* token_service,
    SigninClient* client,
    network::TestURLLoaderFactory* test_url_loader_factory,
    scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
        shared_url_loader_factory)
    : GaiaCookieManagerService(token_service,
                               client,
                               base::BindRepeating(&GetSharedURLLoaderFactory,
                                                   shared_url_loader_factory)),
      test_url_loader_factory_(test_url_loader_factory),
      shared_url_loader_factory_(shared_url_loader_factory) {}

FakeGaiaCookieManagerService::~FakeGaiaCookieManagerService() {
  if (shared_url_loader_factory_)
    shared_url_loader_factory_->Detach();
}

void FakeGaiaCookieManagerService::SetListAccountsResponseHttpNotFound() {
  signin::SetListAccountsResponseHttpNotFound(test_url_loader_factory_);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseWebLoginRequired() {
  signin::SetListAccountsResponseWebLoginRequired(test_url_loader_factory_);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseWithParams(
    const std::vector<signin::CookieParams>& params) {
  signin::SetListAccountsResponseWithParams(params, test_url_loader_factory_);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseNoAccounts() {
  signin::SetListAccountsResponseNoAccounts(test_url_loader_factory_);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseOneAccount(
    const std::string& email,
    const std::string& gaia_id) {
  signin::SetListAccountsResponseOneAccount(email, gaia_id,
                                            test_url_loader_factory_);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseOneAccountWithParams(
    const signin::CookieParams& params) {
  signin::SetListAccountsResponseOneAccountWithParams(params,
                                                      test_url_loader_factory_);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseTwoAccounts(
    const std::string& email1,
    const std::string& gaia_id1,
    const std::string& email2,
    const std::string& gaia_id2) {
  signin::SetListAccountsResponseTwoAccounts(email1, gaia_id1, email2, gaia_id2,
                                             test_url_loader_factory_);
}
