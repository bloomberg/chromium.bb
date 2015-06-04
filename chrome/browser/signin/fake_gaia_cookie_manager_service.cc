// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/fake_gaia_cookie_manager_service.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"

FakeGaiaCookieManagerService::FakeGaiaCookieManagerService(
    OAuth2TokenService* token_service,
    const std::string& source,
    SigninClient* client) :
        GaiaCookieManagerService(token_service, source, client),
        url_fetcher_factory_(NULL) {}

void FakeGaiaCookieManagerService::Init(
    net::FakeURLFetcherFactory* url_fetcher_factory) {
  url_fetcher_factory_ = url_fetcher_factory;
}

void FakeGaiaCookieManagerService::SetListAccountsResponseHttpNotFound() {
  DCHECK(url_fetcher_factory_);
  url_fetcher_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      "",
      net::HTTP_NOT_FOUND,
      net::URLRequestStatus::SUCCESS);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseWebLoginRequired() {
  DCHECK(url_fetcher_factory_);
  url_fetcher_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      "Info=WebLoginRequired",
      net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseNoAccounts() {
  DCHECK(url_fetcher_factory_);
  url_fetcher_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      "[\"f\", []]",
      net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseOneAccount(
    const char* account) {
  DCHECK(url_fetcher_factory_);
  url_fetcher_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      base::StringPrintf(
          "[\"f\", [[\"b\", 0, \"n\", \"%s\", \"p\", 0, 0, 0, 0, 1, \"22\"]]]",
          account),
      net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseOneAccountWithExpiry(
    const char* account, bool expired) {
  DCHECK(url_fetcher_factory_);
  url_fetcher_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      base::StringPrintf(
          "[\"f\", [[\"b\", 0, \"n\", \"%s\", \"p\", 0, 0, 0, 0, %d, \"22\"]]]",
          account, expired ? 0 : 1),
      net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseTwoAccounts(
    const char* account1, const char* account2) {
  DCHECK(url_fetcher_factory_);
  url_fetcher_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      base::StringPrintf(
          "[\"f\", [[\"b\", 0, \"n\", \"%s\", \"p\", 0, 0, 0, 0, 1, \"22\"], "
          "[\"b\", 0, \"n\", \"%s\", \"p\", 0, 0, 0, 0, 1, \"24\"]]]",
          account1, account2),
      net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);
}

void FakeGaiaCookieManagerService::SetListAccountsResponseTwoAccountsWithExpiry(
    const char* account1, bool account1_expired,
    const char* account2, bool account2_expired) {
  DCHECK(url_fetcher_factory_);
  url_fetcher_factory_->SetFakeResponse(
      GaiaUrls::GetInstance()->ListAccountsURLWithSource(
          GaiaConstants::kChromeSource),
      base::StringPrintf(
          "[\"f\", [[\"b\", 0, \"n\", \"%s\", \"p\", 0, 0, 0, 0, %d, \"28\"], "
          "[\"b\", 0, \"n\", \"%s\", \"p\", 0, 0, 0, 0, %d, \"29\"]]]",
          account1, account1_expired ? 0 : 1,
          account2, account2_expired ? 0 : 1),
      net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);
}

// static
KeyedService* FakeGaiaCookieManagerService::Build(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  FakeGaiaCookieManagerService* service = new FakeGaiaCookieManagerService(
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile),
      GaiaConstants::kChromeSource,
      ChromeSigninClientFactory::GetForProfile(profile));
  return service;
}
