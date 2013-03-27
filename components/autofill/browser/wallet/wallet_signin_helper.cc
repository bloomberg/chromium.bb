// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/wallet/wallet_signin_helper.h"

#include "base/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/values.h"
#include "components/autofill/browser/wallet/wallet_service_url.h"
#include "components/autofill/browser/wallet/wallet_signin_helper_delegate.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/escape.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context.h"

namespace autofill {
namespace wallet {

namespace {

// Toolbar::GetAccountInfo API URL (JSON).
const char kGetAccountInfoUrlFormat[] =
    "https://clients1.google.com/tbproxy/getaccountinfo?key=%d&rv=2";

}  // namespace

WalletSigninHelper::WalletSigninHelper(
    WalletSigninHelperDelegate* delegate,
    net::URLRequestContextGetter* getter)
    : delegate_(delegate),
      getter_(getter),
      state_(IDLE) {
  DCHECK(delegate_);
}

WalletSigninHelper::~WalletSigninHelper() {
}

void WalletSigninHelper::StartPassiveSignin() {
  DCHECK_EQ(IDLE, state_);
  DCHECK(!url_fetcher_);
  DCHECK(!gaia_fetcher_);

  state_ = PASSIVE_EXECUTING_SIGNIN;
  sid_.clear();
  lsid_.clear();
  username_.clear();
  const GURL& url = wallet::GetPassiveAuthUrl();
  url_fetcher_.reset(net::URLFetcher::Create(
      0, url, net::URLFetcher::GET, this));
  url_fetcher_->SetRequestContext(getter_);
  url_fetcher_->Start();
}

void WalletSigninHelper::StartAutomaticSignin(
    const std::string& sid, const std::string& lsid) {
  DCHECK(!sid.empty());
  DCHECK(!lsid.empty());
  DCHECK_EQ(state_, IDLE);
  DCHECK(!url_fetcher_);
  DCHECK(!gaia_fetcher_);

  state_ = AUTOMATIC_FETCHING_USERINFO;
  sid_ = sid;
  lsid_ = lsid;
  username_.clear();
  gaia_fetcher_.reset(new GaiaAuthFetcher(
      this, GaiaConstants::kChromeSource, getter_));
  gaia_fetcher_->StartGetUserInfo(lsid_);
}

void WalletSigninHelper::StartUserNameFetch() {
  DCHECK_EQ(state_, IDLE);
  DCHECK(!url_fetcher_);
  DCHECK(!gaia_fetcher_);

  state_ = USERNAME_FETCHING_USERINFO;
  sid_.clear();
  lsid_.clear();
  username_.clear();
  StartFetchingUserNameFromSession();
}

std::string WalletSigninHelper::GetGetAccountInfoUrlForTesting() const {
  return base::StringPrintf(kGetAccountInfoUrlFormat, 0);
}

void WalletSigninHelper::OnServiceError(const GoogleServiceAuthError& error) {
  const State state_with_error = state_;
  state_ = IDLE;
  url_fetcher_.reset();
  gaia_fetcher_.reset();

  switch(state_with_error) {
    case IDLE:
      NOTREACHED();
      break;

    case PASSIVE_EXECUTING_SIGNIN:  /*FALLTHROUGH*/
    case PASSIVE_FETCHING_USERINFO:
      delegate_->OnPassiveSigninFailure(error);
      break;

    case AUTOMATIC_FETCHING_USERINFO:  /*FALLTHROUGH*/
    case AUTOMATIC_ISSUING_AUTH_TOKEN:  /*FALLTHROUGH*/
    case AUTOMATIC_EXECUTING_SIGNIN:
      delegate_->OnAutomaticSigninFailure(error);
      break;

    case USERNAME_FETCHING_USERINFO:
      delegate_->OnUserNameFetchFailure(error);
      break;
  }
}

void WalletSigninHelper::OnOtherError() {
  OnServiceError(GoogleServiceAuthError::AuthErrorNone());
}

void WalletSigninHelper::OnGetUserInfoSuccess(
    const UserInfoMap& data) {
  DCHECK_EQ(AUTOMATIC_FETCHING_USERINFO, state_);

  UserInfoMap::const_iterator email_iter =
      data.find(GaiaConstants::kClientOAuthEmailKey);
  if (email_iter != data.end()) {
    username_ = email_iter->second;
    DCHECK(!url_fetcher_);
    state_ = AUTOMATIC_ISSUING_AUTH_TOKEN;
    gaia_fetcher_.reset(new GaiaAuthFetcher(
        this, GaiaConstants::kChromeSource, getter_));
    gaia_fetcher_->StartIssueAuthToken(
        sid_, lsid_, GaiaConstants::kGaiaService);
  } else {
    LOG(ERROR) << "GetUserInfoFailure: email field not found";
    OnOtherError();
  }
}

void WalletSigninHelper::OnGetUserInfoFailure(
    const GoogleServiceAuthError& error) {
  LOG(ERROR) << "GetUserInfoFailure: " << error.ToString();
  DCHECK_EQ(AUTOMATIC_FETCHING_USERINFO, state_);
  OnServiceError(error);
}

void WalletSigninHelper::OnIssueAuthTokenSuccess(
    const std::string& service,
    const std::string& auth_token) {
  DCHECK_EQ(AUTOMATIC_ISSUING_AUTH_TOKEN, state_);

  state_ = AUTOMATIC_EXECUTING_SIGNIN;
  std::string encoded_auth_token = net::EscapeUrlEncodedData(auth_token, true);
  std::string encoded_continue_url =
      net::EscapeUrlEncodedData(wallet::GetPassiveAuthUrl().spec(), true);
  std::string encoded_source = net::EscapeUrlEncodedData(
      GaiaConstants::kChromeSource, true);
  std::string body = base::StringPrintf(
      "auth=%s&"
      "continue=%s&"
      "source=%s",
      encoded_auth_token.c_str(),
      encoded_continue_url.c_str(),
      encoded_source.c_str());

  gaia_fetcher_.reset();
  DCHECK(!url_fetcher_);
  url_fetcher_.reset(net::URLFetcher::Create(
      0,
      GURL(GaiaUrls::GetInstance()->token_auth_url()),
      net::URLFetcher::POST,
      this));
  url_fetcher_->SetUploadData("application/x-www-form-urlencoded", body);
  url_fetcher_->SetRequestContext(getter_);
  url_fetcher_->Start();  // This will result in OnURLFetchComplete callback.
}

void WalletSigninHelper::OnIssueAuthTokenFailure(
    const std::string& service,
    const GoogleServiceAuthError& error) {
  LOG(ERROR) << "IssueAuthTokenFailure: " << error.ToString();
  DCHECK_EQ(AUTOMATIC_ISSUING_AUTH_TOKEN, state_);
  OnServiceError(error);
}

void WalletSigninHelper::OnURLFetchComplete(
    const net::URLFetcher* fetcher) {
  DCHECK_EQ(url_fetcher_.get(), fetcher);
  DCHECK(!gaia_fetcher_);
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
      url_fetcher_.reset();
      state_ = PASSIVE_FETCHING_USERINFO;
      StartFetchingUserNameFromSession();
      break;

    case AUTOMATIC_EXECUTING_SIGNIN:
      state_ = IDLE;
      url_fetcher_.reset();
      delegate_->OnAutomaticSigninSuccess(username_);
      break;

    default:
      NOTREACHED() << "unexpected state_=" << state_;
  }
}

void WalletSigninHelper::StartFetchingUserNameFromSession() {
  DCHECK(!gaia_fetcher_);
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

bool WalletSigninHelper::ParseGetAccountInfoResponse(
    const net::URLFetcher* fetcher, std::string* email) {
  DCHECK(email);

  std::string data;
  if (!fetcher->GetResponseAsString(&data)) {
    LOG(ERROR) << "failed to GetResponseAsString";
    return false;
  }

  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  if (!value.get() || value->GetType() != base::Value::TYPE_DICTIONARY) {
    LOG(ERROR) << "failed to parse JSON response";
    return false;
  }

  DictionaryValue* dict = static_cast<DictionaryValue*>(value.get());
  if (!dict->GetStringWithoutPathExpansion("email", email)) {
    LOG(ERROR) << "no email in JSON response";
    return false;
  }

  return !email->empty();
}

}  // namespace wallet
}  // namespace autofill
