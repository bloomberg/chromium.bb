// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"

FakeGaiaCookieManagerService::FakeGaiaCookieManagerService(
    OAuth2TokenService* token_service,
    const std::string& source,
    SigninClient* client)
    : GaiaCookieManagerService(token_service, source, client),
      url_fetcher_factory_(nullptr) {}

void FakeGaiaCookieManagerService::Init(
    net::FakeURLFetcherFactory* url_fetcher_factory) {
  url_fetcher_factory_ = url_fetcher_factory;
}

void FakeGaiaCookieManagerService::SetListAccountsResponseHttpNotFound() {
  DCHECK(url_fetcher_factory_);
  url_fetcher_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      "", net::HTTP_NOT_FOUND, net::URLRequestStatus::SUCCESS);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseWebLoginRequired() {
  DCHECK(url_fetcher_factory_);
  url_fetcher_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      "Info=WebLoginRequired", net::HTTP_OK, net::URLRequestStatus::SUCCESS);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseWithParams(
    const std::vector<CookieParams>& params) {
  DCHECK(url_fetcher_factory_);

  std::vector<std::string> response_body;
  for (const auto& param : params) {
    std::string response_part = base::StringPrintf(
        "[\"b\", 0, \"n\", \"%s\", \"p\", 0, 0, 0, 0, %d, \"%s\"",
        param.email.c_str(), param.valid ? 1 : 0, param.gaia_id.c_str());
    if (param.signed_out || !param.verified) {
      response_part +=
          base::StringPrintf(", null, null, null, %d, %d",
                             param.signed_out ? 1 : 0, param.verified ? 1 : 0);
    }
    response_part += "]";
    response_body.push_back(response_part);
  }

  url_fetcher_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      std::string("[\"f\", [") + base::JoinString(response_body, ", ") + "]]",
      net::HTTP_OK, net::URLRequestStatus::SUCCESS);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseNoAccounts() {
  SetListAccountsResponseWithParams({});
}

void FakeGaiaCookieManagerService::SetListAccountsResponseOneAccount(
    const std::string& email,
    const std::string& gaia_id) {
  CookieParams params = {email, gaia_id, true /* valid */,
                         false /* signed_out */, true /* verified */};
  SetListAccountsResponseWithParams({params});
}

void FakeGaiaCookieManagerService::SetListAccountsResponseOneAccountWithParams(
    const CookieParams& params) {
  SetListAccountsResponseWithParams({params});
}

void FakeGaiaCookieManagerService::SetListAccountsResponseTwoAccounts(
    const std::string& email1,
    const std::string& gaia_id1,
    const std::string& email2,
    const std::string& gaia_id2) {
  SetListAccountsResponseWithParams(
      {{email1, gaia_id1, true /* valid */, false /* signed_out */,
        true /* verified */},
       {email2, gaia_id2, true /* valid */, false /* signed_out */,
        true /* verified */}});
}

std::string FakeGaiaCookieManagerService::GetSourceForRequest(
    const GaiaCookieManagerService::GaiaCookieRequest& request) {
  // Always return the default.  This value must match the source used in the
  // SetXXXResponseYYY methods above so that the test URLFetcher factory will
  // be able to find the URLs.
  return GaiaConstants::kChromeSource;
}

std::string FakeGaiaCookieManagerService::GetDefaultSourceForRequest() {
  // Always return the default.  This value must match the source used in the
  // SetXXXResponseYYY methods above so that the test URLFetcher factory will
  // be able to find the URLs.
  return GaiaConstants::kChromeSource;
}
