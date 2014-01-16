// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/authenticated_user_email_retriever.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

scoped_refptr<net::CookieStore> GetCookieStoreOnIO(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  return url_request_context_getter->GetURLRequestContext()->cookie_store();
}

}  // namespace

AuthenticatedUserEmailRetriever::AuthenticatedUserEmailRetriever(
    const AuthenticatedUserEmailCallback& callback,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter)
    : callback_(callback),
      url_request_context_getter_(url_request_context_getter),
      weak_factory_(this) {
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GetCookieStoreOnIO, url_request_context_getter),
      base::Bind(&AuthenticatedUserEmailRetriever::ExtractCookies,
                 weak_factory_.GetWeakPtr()));
}

AuthenticatedUserEmailRetriever::~AuthenticatedUserEmailRetriever() {
}

void AuthenticatedUserEmailRetriever::OnGetUserInfoSuccess(
    const UserInfoMap& data) {
  UserInfoMap::const_iterator it = data.find("email");
  callback_.Run(it != data.end() ? it->second : "");
}

void AuthenticatedUserEmailRetriever::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  callback_.Run(std::string());
}

void AuthenticatedUserEmailRetriever::ExtractCookies(
    scoped_refptr<net::CookieStore> cookie_store) {
  if (!cookie_store) {
    callback_.Run(std::string());
    return;
  }
  cookie_store->GetCookieMonster()->GetAllCookiesForURLAsync(
      GaiaUrls::GetInstance()->gaia_url(),
      base::Bind(&AuthenticatedUserEmailRetriever::ExtractLSIDFromCookies,
                 weak_factory_.GetWeakPtr()));
}

void AuthenticatedUserEmailRetriever::ExtractLSIDFromCookies(
    const net::CookieList& cookies) {
  for (net::CookieList::const_iterator it = cookies.begin();
       it != cookies.end(); ++it) {
    if (it->Name() == "LSID") {
      gaia_auth_fetcher_.reset(new GaiaAuthFetcher(
          this,
          GaiaConstants::kChromeSource,
          url_request_context_getter_));
      gaia_auth_fetcher_->StartGetUserInfo(it->Value());
      return;
    }
  }
  callback_.Run(std::string());
}

}  // namespace chromeos
