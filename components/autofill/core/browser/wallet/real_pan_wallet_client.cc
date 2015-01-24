// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/wallet/real_pan_wallet_client.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "net/base/escape.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace autofill {
namespace wallet {

namespace {

const char kUnmaskCardRequestFormat[] =
    "request_content_type=application/json&request=%s&cvc=%s";

// TODO(estade): use a sandbox server on dev builds?
const char kUnmaskCardRequestUrl[] =
    "https://wallet.google.com/payments/apis-secure/creditcardservice"
    "/GetRealPan?s7e=cvc";

}  // namespace

RealPanWalletClient::RealPanWalletClient(
    net::URLRequestContextGetter* context_getter,
    Delegate* delegate)
    : context_getter_(context_getter),
      delegate_(delegate),
      weak_ptr_factory_(this) {
  DCHECK(delegate);
}

RealPanWalletClient::~RealPanWalletClient() {
}

void RealPanWalletClient::UnmaskCard(const CreditCard& card,
                                     const std::string& cvc) {
  request_.reset(net::URLFetcher::Create(
      0, GURL(kUnmaskCardRequestUrl), net::URLFetcher::POST, this));
  request_->SetRequestContext(context_getter_.get());

  base::DictionaryValue request_dict;
  request_dict.SetString("encrypted_cvc", "__param:cvc");
  // TODO(estade): get the token from |card|.
  request_dict.SetString("credit_card_token", "deadbeefee");
  std::string json_request;
  base::JSONWriter::Write(&request_dict, &json_request);
  std::string post_body = base::StringPrintf(kUnmaskCardRequestFormat,
      net::EscapeUrlEncodedData(json_request, true).c_str(),
      net::EscapeUrlEncodedData(cvc, true).c_str());
  request_->SetUploadData("application/x-www-form-urlencoded", post_body);

  request_->AddExtraRequestHeader(
      "Authorization: " + delegate_->GetOAuth2Token());
  request_->Start();
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

  switch (response_code) {
    // Valid response.
    case net::HTTP_OK: {
      std::string data;
      source->GetResponseAsString(&data);
      scoped_ptr<base::Value> message_value(base::JSONReader::Read(data));
      if (message_value.get() &&
          message_value->IsType(base::Value::TYPE_DICTIONARY)) {
        response_dict.reset(
            static_cast<base::DictionaryValue*>(message_value.release()));
      } else {
        NOTIMPLEMENTED();
      }
      break;
    }

    // HTTP_BAD_REQUEST means the arguments are invalid. No point retrying.
    case net::HTTP_BAD_REQUEST: {
      NOTIMPLEMENTED();
      break;
    }

    // Response contains an error to show the user.
    case net::HTTP_FORBIDDEN:
    case net::HTTP_INTERNAL_SERVER_ERROR: {
      NOTIMPLEMENTED();
      break;
    }

    // Handle anything else as a generic error.
    default:
      NOTIMPLEMENTED();
      break;
  }

  std::string real_pan;
  if (response_dict)
    response_dict->GetString("pan", &real_pan);
  delegate_->OnDidGetRealPan(real_pan);
}

}  // namespace wallet
}  // namespace autofill
