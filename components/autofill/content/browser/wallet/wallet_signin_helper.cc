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

// Toolbar::GetAccountInfo API URL (JSON).
const char kGetAccountInfoUrlFormat[] =
    "https://clients1.google.com/tbproxy/getaccountinfo?key=%d&rv=2&requestor=chrome";

const char kWalletCookieName[] = "gdtoken";

// Callback for retrieving Google Wallet cookies. |callback| is passed the
// retrieved cookies and posted back to the UI thread. |cookies| is any Google
// Wallet cookies.
void GetGoogleCookiesCallback(
    const base::Callback<void(const std::string&)>& callback,
    const net::CookieList& cookies) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  std::string wallet_cookie;
  for (size_t i = 0; i < cookies.size(); ++i) {
    if (LowerCaseEqualsASCII(cookies[i].Name(), kWalletCookieName)) {
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
  if (!url_request_context) {
    content::BrowserThread::PostTask(content::BrowserThread::UI,
                                     FROM_HERE,
                                     base::Bind(callback, std::string()));
    return;
  }

  net::CookieStore* cookie_store = url_request_context->cookie_store();
  if (!cookie_store) {
    content::BrowserThread::PostTask(content::BrowserThread::UI,
                                     FROM_HERE,
                                     base::Bind(callback, std::string()));
    return;
  }

  net::CookieMonster* cookie_monster = cookie_store->GetCookieMonster();
  if (!cookie_monster) {
    content::BrowserThread::PostTask(content::BrowserThread::UI,
                                     FROM_HERE,
                                     base::Bind(callback, std::string()));
    return;
  }

  net::CookieOptions cookie_options;
  cookie_options.set_include_httponly();
  cookie_monster->GetAllCookiesForURLWithOptionsAsync(
      wallet::GetPassiveAuthUrl().GetWithEmptyPath(),
      cookie_options,
      base::Bind(&GetGoogleCookiesCallback, callback));
}

}  // namespace

WalletSigninHelper::WalletSigninHelper(
    WalletSigninHelperDelegate* delegate,
    net::URLRequestContextGetter* getter)
    : delegate_(delegate),
      getter_(getter),
      state_(IDLE),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
}

WalletSigninHelper::~WalletSigninHelper() {
}

void WalletSigninHelper::StartPassiveSignin() {
  DCHECK_EQ(IDLE, state_);
  DCHECK(!url_fetcher_);

  state_ = PASSIVE_EXECUTING_SIGNIN;
  username_.clear();
  const GURL& url = wallet::GetPassiveAuthUrl();
  url_fetcher_.reset(net::URLFetcher::Create(
      0, url, net::URLFetcher::GET, this));
  url_fetcher_->SetRequestContext(getter_);
  url_fetcher_->Start();
}

void WalletSigninHelper::StartUserNameFetch() {
  DCHECK_EQ(state_, IDLE);
  DCHECK(!url_fetcher_);

  state_ = USERNAME_FETCHING_USERINFO;
  username_.clear();
  StartFetchingUserNameFromSession();
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

std::string WalletSigninHelper::GetGetAccountInfoUrlForTesting() const {
  return base::StringPrintf(kGetAccountInfoUrlFormat, 0);
}

void WalletSigninHelper::OnServiceError(const GoogleServiceAuthError& error) {
  const State state_with_error = state_;
  state_ = IDLE;
  url_fetcher_.reset();

  switch(state_with_error) {
    case IDLE:
      NOTREACHED();
      break;

    case PASSIVE_EXECUTING_SIGNIN:  /*FALLTHROUGH*/
    case PASSIVE_FETCHING_USERINFO:
      delegate_->OnPassiveSigninFailure(error);
      break;

    case USERNAME_FETCHING_USERINFO:
      delegate_->OnUserNameFetchFailure(error);
      break;
  }
}

void WalletSigninHelper::OnOtherError() {
  OnServiceError(GoogleServiceAuthError::AuthErrorNone());
}

void WalletSigninHelper::OnURLFetchComplete(
    const net::URLFetcher* fetcher) {
  DCHECK_EQ(url_fetcher_.get(), fetcher);
  if (!fetcher->GetStatus().is_success() ||
      fetcher->GetResponseCode() < 200 ||
      fetcher->GetResponseCode() >= 300) {
    LOG(ERROR) << "URLFetchFailure: state=" << state_
               << " r=" << fetcher->GetResponseCode()
               << " s=" << fetcher->GetStatus().status()
               << " e=" << fetcher->GetStatus().error();
    OnOtherError();
    return;
  }

  switch (state_) {
    case USERNAME_FETCHING_USERINFO:  /*FALLTHROUGH*/
    case PASSIVE_FETCHING_USERINFO:
      ProcessGetAccountInfoResponseAndFinish();
      break;

    case PASSIVE_EXECUTING_SIGNIN:
      if (ParseSignInResponse()) {
        url_fetcher_.reset();
        state_ = PASSIVE_FETCHING_USERINFO;
        StartFetchingUserNameFromSession();
      }
      break;

    default:
      NOTREACHED() << "unexpected state_=" << state_;
  }
}

void WalletSigninHelper::StartFetchingUserNameFromSession() {
  const int random_number = static_cast<int>(base::RandUint64() % INT_MAX);
  url_fetcher_.reset(
      net::URLFetcher::Create(
          0,
          GURL(base::StringPrintf(kGetAccountInfoUrlFormat, random_number)),
          net::URLFetcher::GET,
          this));
  url_fetcher_->SetRequestContext(getter_);
  url_fetcher_->Start();  // This will result in OnURLFetchComplete callback.
}

void WalletSigninHelper::ProcessGetAccountInfoResponseAndFinish() {
  std::string email;
  if (!ParseGetAccountInfoResponse(url_fetcher_.get(), &email)) {
    LOG(ERROR) << "failed to get the user email";
    OnOtherError();
    return;
  }

  username_ = email;
  const State finishing_state = state_;
  state_ = IDLE;
  url_fetcher_.reset();
  switch(finishing_state) {
    case USERNAME_FETCHING_USERINFO:
      delegate_->OnUserNameFetchSuccess(username_);
      break;

    case PASSIVE_FETCHING_USERINFO:
      delegate_->OnPassiveSigninSuccess(username_);
      break;

    default:
      NOTREACHED() << "unexpected state_=" << finishing_state;
  }
}

bool WalletSigninHelper::ParseSignInResponse() {
  if (!url_fetcher_) {
    NOTREACHED();
    return false;
  }

  std::string data;
  if (!url_fetcher_->GetResponseAsString(&data)) {
    DVLOG(1) << "failed to GetResponseAsString";
    OnOtherError();
    return false;
  }

  if (!LowerCaseEqualsASCII(data, "yes")) {
    OnServiceError(
        GoogleServiceAuthError(GoogleServiceAuthError::USER_NOT_SIGNED_UP));
    return false;
  }

  return true;
}

bool WalletSigninHelper::ParseGetAccountInfoResponse(
    const net::URLFetcher* fetcher, std::string* email) {
  DCHECK(email);

  std::string data;
  if (!fetcher->GetResponseAsString(&data)) {
    DVLOG(1) << "failed to GetResponseAsString";
    return false;
  }

  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  if (!value.get() || value->GetType() != base::Value::TYPE_DICTIONARY) {
    DVLOG(1) << "failed to parse JSON response";
    return false;
  }

  DictionaryValue* dict = static_cast<base::DictionaryValue*>(value.get());
  base::ListValue* user_info;
  if (!dict->GetListWithoutPathExpansion("user_info", &user_info)) {
    DVLOG(1) << "no user_info in JSON response";
    return false;
  }

  // |user_info| will contain each signed in user in the cookie jar.
  // We only support the first user at the moment. http://crbug.com/259543
  // will change that.
  base::DictionaryValue* user_info_detail;
  if (!user_info->GetDictionary(0, &user_info_detail)) {
    DVLOG(1) << "empty list in JSON response";
    return false;
  }

  if (!user_info_detail->GetStringWithoutPathExpansion("email", email)) {
    DVLOG(1) << "no email in JSON response";
    return false;
  }

  return !email->empty();
}

void WalletSigninHelper::ReturnWalletCookieValue(
    const std::string& cookie_value) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  delegate_->OnDidFetchWalletCookieValue(cookie_value);
}

}  // namespace wallet
}  // namespace autofill
