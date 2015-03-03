// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/wallet/real_pan_wallet_client.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "google_apis/gaia/identity_provider.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace autofill {
namespace wallet {

namespace {

const char kUnmaskCardRequestFormat[] =
    "requestContentType=application/json; charset=utf-8&request=%s"
    "&s7e_13_cvc=%s";

const char kUnmaskCardRequestHost[] = "https://wallet.google.com";
const char kUnmaskCardRequestHostSandbox[] = "https://sandbox.google.com";
const char kUnmaskCardRequestPath[] =
    "payments/apis-secure/creditcardservice/getrealpan?s7e_suffix=chromewallet";

const char kTokenServiceConsumerId[] = "real_pan_wallet_client";
const char kWalletOAuth2Scope[] =
    "https://www.googleapis.com/auth/wallet.chrome";

// This is mostly copied from wallet_service_url.cc, which is currently in
// content/, hence inaccessible from here.
bool IsWalletProductionEnabled() {
  // If the command line flag exists, it takes precedence.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string sandbox_enabled(
      command_line->GetSwitchValueASCII(switches::kWalletServiceUseSandbox));
  if (!sandbox_enabled.empty())
    return sandbox_enabled != "1";

#if defined(ENABLE_PROD_WALLET_SERVICE)
  return true;
#else
  return false;
#endif
}

GURL GetUnmaskCardRequestUrl() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch("sync-url")) {
    if (IsWalletProductionEnabled()) {
      LOG(ERROR) << "You are using production Wallet but you specified a "
                    "--sync-url. You likely want to disable the sync sandbox "
                    "or switch to sandbox Wallet. Both are controlled in "
                    "about:flags.";
    }
  } else if (!IsWalletProductionEnabled()) {
    LOG(ERROR) << "You are using sandbox Wallet but you didn't specify a "
                  "--sync-url. You likely want to enable the sync sandbox "
                  "or switch to production Wallet. Both are controlled in "
                  "about:flags.";
  }

  GURL base(IsWalletProductionEnabled() ? kUnmaskCardRequestHost
                                        : kUnmaskCardRequestHostSandbox);
  return base.Resolve(kUnmaskCardRequestPath);
}

}  // namespace

RealPanWalletClient::RealPanWalletClient(
    net::URLRequestContextGetter* context_getter,
    Delegate* delegate)
    : OAuth2TokenService::Consumer(kTokenServiceConsumerId),
      context_getter_(context_getter),
      delegate_(delegate),
      has_retried_authorization_(false),
      weak_ptr_factory_(this) {
  DCHECK(delegate);
}

RealPanWalletClient::~RealPanWalletClient() {
}

void RealPanWalletClient::Prepare() {
  if (access_token_.empty())
    StartTokenFetch(false);
}

void RealPanWalletClient::UnmaskCard(
    const CreditCard& card,
    const CardUnmaskDelegate::UnmaskResponse& response) {
  DCHECK_EQ(CreditCard::MASKED_SERVER_CARD, card.record_type());
  card_ = card;
  response_ = response;
  has_retried_authorization_ = false;

  CreateRequest();
  if (access_token_.empty())
    StartTokenFetch(false);
  else
    SetOAuth2TokenAndStartRequest();
}

void RealPanWalletClient::CancelRequest() {
  request_.reset();
}

void RealPanWalletClient::OnURLFetchComplete(const net::URLFetcher* source) {
  DCHECK_EQ(source, request_.get());

  // |request_|, which is aliased to |source|, might continue to be used in this
  // |method, but should be freed once control leaves the method.
  scoped_ptr<net::URLFetcher> scoped_request(request_.Pass());
  scoped_ptr<base::DictionaryValue> response_dict;
  int response_code = source->GetResponseCode();
  std::string data;
  source->GetResponseAsString(&data);

  std::string real_pan;
  AutofillClient::GetRealPanResult result = AutofillClient::SUCCESS;

  switch (response_code) {
    // Valid response.
    case net::HTTP_OK: {
      scoped_ptr<base::Value> message_value(base::JSONReader::Read(data));
      if (message_value.get() &&
          message_value->IsType(base::Value::TYPE_DICTIONARY)) {
        response_dict.reset(
            static_cast<base::DictionaryValue*>(message_value.release()));
        response_dict->GetString("pan", &real_pan);
        if (real_pan.empty())
          result = AutofillClient::TRY_AGAIN_FAILURE;
        // TODO(estade): check response for allow_retry.
      }
      break;
    }

    case net::HTTP_UNAUTHORIZED: {
      if (has_retried_authorization_) {
        result = AutofillClient::PERMANENT_FAILURE;
        break;
      }
      has_retried_authorization_ = true;

      CreateRequest();
      StartTokenFetch(true);
      return;
    }

    // TODO(estade): is this actually how network connectivity issues are
    // reported?
    case net::HTTP_REQUEST_TIMEOUT: {
      result = AutofillClient::NETWORK_ERROR;
      break;
    }

    // Handle anything else as a generic (permanent) failure.
    default: {
      result = AutofillClient::PERMANENT_FAILURE;
      break;
    }
  }

  if (real_pan.empty()) {
    LOG(ERROR) << "Wallet returned error: " << response_code
               << " with data: " << data;
  }

  DCHECK_EQ(result != AutofillClient::SUCCESS, real_pan.empty());
  delegate_->OnDidGetRealPan(result, real_pan);
}

void RealPanWalletClient::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  DCHECK_EQ(request, access_token_request_.get());
  access_token_ = access_token;
  if (request_)
    SetOAuth2TokenAndStartRequest();

  access_token_request_.reset();
}

void RealPanWalletClient::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(request, access_token_request_.get());
  LOG(ERROR) << "Unhandled OAuth2 error: " << error.ToString();
  if (request_) {
    request_.reset();
    delegate_->OnDidGetRealPan(AutofillClient::PERMANENT_FAILURE,
                               std::string());
  }
  access_token_request_.reset();
}

void RealPanWalletClient::CreateRequest() {
  request_.reset(net::URLFetcher::Create(
      0, GetUnmaskCardRequestUrl(), net::URLFetcher::POST, this));
  request_->SetRequestContext(context_getter_.get());
  request_->SetLoadFlags(net::LOAD_DO_NOT_SAVE_COOKIES |
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DISABLE_CACHE);

  base::DictionaryValue request_dict;
  request_dict.SetString("encrypted_cvc", "__param:s7e_13_cvc");
  request_dict.SetString("credit_card_id", card_.server_id());
  request_dict.SetString("risk_data_base64", response_.risk_data);
  request_dict.Set("context", make_scoped_ptr(new base::DictionaryValue()));

  int value = 0;
  if (base::StringToInt(response_.exp_month, &value))
    request_dict.SetInteger("expiration_month", value);
  if (base::StringToInt(response_.exp_year, &value))
    request_dict.SetInteger("expiration_year", value);

  std::string json_request;
  base::JSONWriter::Write(&request_dict, &json_request);
  std::string post_body =
      base::StringPrintf(kUnmaskCardRequestFormat,
                         net::EscapeUrlEncodedData(json_request, true).c_str(),
                         net::EscapeUrlEncodedData(
                             base::UTF16ToASCII(response_.cvc), true).c_str());
  request_->SetUploadData("application/x-www-form-urlencoded", post_body);
}

void RealPanWalletClient::StartTokenFetch(bool invalidate_old) {
  // We're still waiting for the last request to come back.
  if (!invalidate_old && access_token_request_)
    return;

  OAuth2TokenService::ScopeSet wallet_scopes;
  wallet_scopes.insert(kWalletOAuth2Scope);
  IdentityProvider* identity = delegate_->GetIdentityProvider();
  if (invalidate_old) {
    DCHECK(!access_token_.empty());
    identity->GetTokenService()->InvalidateToken(
        identity->GetActiveAccountId(), wallet_scopes, access_token_);
  }
  access_token_.clear();
  access_token_request_ = identity->GetTokenService()->StartRequest(
      identity->GetActiveAccountId(), wallet_scopes, this);
}

void RealPanWalletClient::SetOAuth2TokenAndStartRequest() {
  request_->AddExtraRequestHeader(net::HttpRequestHeaders::kAuthorization +
                                  std::string(": Bearer ") + access_token_);

  request_->Start();
}

}  // namespace wallet
}  // namespace autofill
