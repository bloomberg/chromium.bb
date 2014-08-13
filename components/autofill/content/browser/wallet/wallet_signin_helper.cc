// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/wallet/wallet_signin_helper.h"

#include "base/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill/content/browser/wallet/wallet_service_url.h"
#include "components/autofill/content/browser/wallet/wallet_signin_helper_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/escape.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

namespace autofill {
namespace wallet {

namespace {

const char kWalletCookieName[] = "gdtoken";

// Callback for retrieving Google Wallet cookies. |callback| is passed the
// retrieved cookies and posted back to the UI thread. |cookies| is any Google
// Wallet cookies.
void GetGoogleCookiesCallback(
    const base::Callback<void(const std::string&)>& callback,
    const net::CookieList& cookies) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  // Cookies for parent domains will also be returned; we only want cookies with
  // exact host matches. TODO(estade): really?
  std::string host = wallet::GetPassiveAuthUrl(0).host();
  std::string wallet_cookie;
  for (size_t i = 0; i < cookies.size(); ++i) {
    if (base::LowerCaseEqualsASCII(cookies[i].Name(), kWalletCookieName) &&
        base::LowerCaseEqualsASCII(cookies[i].Domain(), host.c_str())) {
      wallet_cookie = cookies[i].Value();
      break;
    }
  }
  content::BrowserThread::PostTask(content::BrowserThread::UI,
                                   FROM_HERE,
                                   base::Bind(callback, wallet_cookie));
}

// Gets Google Wallet cookies. Must be called on the IO thread.
// |request_context_getter| is a getter for the current request context.
// |callback| is called when retrieving cookies is completed.
void GetGoogleCookies(
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    const base::Callback<void(const std::string&)>& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  net::URLRequestContext* url_request_context =
      request_context_getter->GetURLRequestContext();
  net::CookieStore* cookie_store = url_request_context ?
      url_request_context->cookie_store() : NULL;
  net::CookieMonster* cookie_monster = cookie_store ?
      cookie_store->GetCookieMonster() : NULL;
  if (!cookie_monster) {
    content::BrowserThread::PostTask(content::BrowserThread::UI,
                                     FROM_HERE,
                                     base::Bind(callback, std::string()));
    return;
  }

  net::CookieOptions cookie_options;
  cookie_options.set_include_httponly();
  cookie_monster->GetAllCookiesForURLWithOptionsAsync(
      wallet::GetPassiveAuthUrl(0).GetWithEmptyPath(),
      cookie_options,
      base::Bind(&GetGoogleCookiesCallback, callback));
}

}  // namespace

WalletSigninHelper::WalletSigninHelper(
    WalletSigninHelperDelegate* delegate,
    net::URLRequestContextGetter* getter)
    : delegate_(delegate),
      getter_(getter),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
}

WalletSigninHelper::~WalletSigninHelper() {
}

void WalletSigninHelper::StartPassiveSignin(size_t user_index) {
  DCHECK(!url_fetcher_);

  const GURL& url = wallet::GetPassiveAuthUrl(user_index);
  url_fetcher_.reset(net::URLFetcher::Create(
      0, url, net::URLFetcher::GET, this));
  url_fetcher_->SetRequestContext(getter_);
  url_fetcher_->Start();
}

void WalletSigninHelper::StartWalletCookieValueFetch() {
  scoped_refptr<net::URLRequestContextGetter> request_context(getter_);
  if (!request_context.get()) {
    ReturnWalletCookieValue(std::string());
    return;
  }

  base::Callback<void(const std::string&)> callback = base::Bind(
      &WalletSigninHelper::ReturnWalletCookieValue,
      weak_ptr_factory_.GetWeakPtr());

  base::Closure task = base::Bind(&GetGoogleCookies, request_context, callback);
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE, task);
}

void WalletSigninHelper::OnServiceError(const GoogleServiceAuthError& error) {
  delegate_->OnPassiveSigninFailure(error);
}

void WalletSigninHelper::OnOtherError() {
  OnServiceError(GoogleServiceAuthError::AuthErrorNone());
}

void WalletSigninHelper::OnURLFetchComplete(
    const net::URLFetcher* fetcher) {
  DCHECK_EQ(url_fetcher_.get(), fetcher);
  scoped_ptr<net::URLFetcher> url_fetcher(url_fetcher_.release());

  if (!fetcher->GetStatus().is_success() ||
      fetcher->GetResponseCode() < 200 ||
      fetcher->GetResponseCode() >= 300) {
    DVLOG(1) << "URLFetchFailure:"
             << " r=" << fetcher->GetResponseCode()
             << " s=" << fetcher->GetStatus().status()
             << " e=" << fetcher->GetStatus().error();
    OnOtherError();
    return;
  }

  std::string data;
  if (!fetcher->GetResponseAsString(&data)) {
    DVLOG(1) << "failed to GetResponseAsString";
    OnOtherError();
    return;
  }

  if (!base::LowerCaseEqualsASCII(data, "yes")) {
    OnServiceError(
        GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP));
    return;
  }

  delegate_->OnPassiveSigninSuccess();
}

void WalletSigninHelper::ReturnWalletCookieValue(
    const std::string& cookie_value) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  delegate_->OnDidFetchWalletCookieValue(cookie_value);
}

}  // namespace wallet
}  // namespace autofill
