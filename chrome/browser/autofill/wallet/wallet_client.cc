// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/wallet/wallet_client.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/autofill/wallet/cart.h"
#include "chrome/browser/autofill/wallet/full_wallet.h"
#include "chrome/browser/autofill/wallet/wallet_address.h"
#include "chrome/browser/autofill/wallet/wallet_items.h"
#include "chrome/browser/autofill/wallet/wallet_service_url.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

const char kEncryptOtpBodyFormat[] = "cvv=%s:%s";
const char kEscrowSensitiveInformationFormat[] = "gid=%s&cardNumber=%s&cvv=%s";
const char kJsonMimeType[] = "application/json";
const char kApplicationMimeType[] = "application/x-www-form-urlencoded";
const size_t kMaxBits = 63;

}  // anonymous namespace

namespace wallet {

void WalletClient::AcceptLegalDocuments(
    const std::vector<std::string>& document_ids,
    const std::string& google_transaction_id,
    WalletClient::WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = ACCEPT_LEGAL_DOCUMENTS;

  DictionaryValue request_dict;
  request_dict.SetString("api_key", wallet::kApiKey);
  request_dict.SetString("google_transaction_id", google_transaction_id);
  ListValue* docs_list = new ListValue();
  for (std::vector<std::string>::const_iterator it = document_ids.begin();
       it != document_ids.end();
       ++it) {
    docs_list->AppendString(*it);
  }
  request_dict.Set("accepted_legal_document", docs_list);

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetAcceptLegalDocumentsUrl(),
                    post_body,
                    observer,
                    kJsonMimeType);
}

void WalletClient::EncryptOtp(
    const void* otp,
    size_t length,
    WalletClient::WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  size_t num_bits = length * 8;
  DCHECK_LT(num_bits, kMaxBits);

  request_type_ = ENCRYPT_OTP;

  std::string post_body = StringPrintf(kEncryptOtpBodyFormat,
                                       base::HexEncode(&num_bits, 1).c_str(),
                                       base::HexEncode(otp, length).c_str());

  MakeWalletRequest(GetEncryptionUrl(),
                    post_body,
                    observer,
                    kApplicationMimeType);
}

void WalletClient::EscrowSensitiveInformation(
    const std::string& primary_account_number,
    const std::string& card_verification_number,
    const std::string& obfuscated_gaia_id,
    WalletClient::WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = ESCROW_SENSITIVE_INFORMATION;

  std::string post_body = StringPrintf(kEscrowSensitiveInformationFormat,
                                       obfuscated_gaia_id.c_str(),
                                       primary_account_number.c_str(),
                                       card_verification_number.c_str());

  MakeWalletRequest(GetEscrowUrl(), post_body, observer, kApplicationMimeType);
}


void WalletClient::GetFullWallet(
    const std::string& instrument_id,
    const std::string& address_id,
    const std::string& merchant_domain,
    const Cart& cart,
    const std::string& google_transaction_id,
    const std::string& encrypted_otp,
    const std::string& session_material,
    WalletClient::WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = GET_FULL_WALLET;

  DictionaryValue request_dict;
  request_dict.SetString("api_key", wallet::kApiKey);
  request_dict.SetString("risk_params", GetRiskParams());
  request_dict.SetString("selected_instrument_id", instrument_id);
  request_dict.SetString("selected_address_id", address_id);
  request_dict.SetString("merchant_domain", merchant_domain);
  request_dict.SetString("google_transaction_id", google_transaction_id);
  request_dict.Set("cart", cart.ToDictionary().release());
  request_dict.SetString("encrypted_otp", encrypted_otp);
  request_dict.SetString("session_material", session_material);

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetGetFullWalletUrl(), post_body, observer, kJsonMimeType);
}

void WalletClient::GetWalletItems(
    WalletClient::WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = GET_WALLET_ITEMS;

  DictionaryValue request_dict;
  request_dict.SetString("api_key", wallet::kApiKey);
  request_dict.SetString("risk_params", GetRiskParams());

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetGetWalletItemsUrl(), post_body, observer, kJsonMimeType);
}

void WalletClient::SendExtendedAutofillStatus(
    bool success,
    const std::string& merchant_domain,
    const std::string& reason,
    const std::string& google_transaction_id,
    WalletClient::WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = SEND_STATUS;

  DictionaryValue request_dict;
  request_dict.SetString("api_key", wallet::kApiKey);
  request_dict.SetBoolean("success", success);
  request_dict.SetString("hostname", merchant_domain);
  if (!success) {
    // TODO(ahutter): Probably want to do some checks on reason.
    request_dict.SetString("reason", reason);
  }
  request_dict.SetString("google_transaction_id", google_transaction_id);

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetSendStatusUrl(), post_body, observer, kJsonMimeType);
}

void WalletClient::MakeWalletRequest(
    const GURL& url,
    const std::string& post_body,
    WalletClient::WalletClientObserver* observer,
    const std::string& content_type) {
  DCHECK(!request_.get()) << "Tried to fetch two things at once!";
  DCHECK(observer);

  observer_ = observer;

  request_.reset(net::URLFetcher::Create(
      0, url, net::URLFetcher::POST, this));
  request_->SetRequestContext(context_getter_);
  DVLOG(1) << "Request body: " << post_body;
  request_->SetUploadData(content_type, post_body);
  request_->Start();
}

// TODO(ahutter): Add manual retry logic if it's necessary.
void WalletClient::OnURLFetchComplete(
    const net::URLFetcher* source) {
  scoped_ptr<net::URLFetcher> old_request = request_.Pass();
  DCHECK_EQ(source, old_request.get());

  DVLOG(1) << "Got response from " << source->GetOriginalURL();

  std::string data;
  scoped_ptr<DictionaryValue> response_dict;
  int response_code = source->GetResponseCode();
  switch (response_code) {
    // HTTP_BAD_REQUEST means the arguments are invalid. No point retrying.
    case net::HTTP_BAD_REQUEST: {
      request_type_ = NO_PENDING_REQUEST;
      observer_->OnWalletError();
      return;
    }
    // HTTP_OK holds a valid response and HTTP_INTERNAL_SERVER_ERROR holds an
    // error code and message for the user.
    case net::HTTP_OK:
    case net::HTTP_INTERNAL_SERVER_ERROR: {
      source->GetResponseAsString(&data);
      DVLOG(1) << "Response body: " << data;
      scoped_ptr<Value> message_value(base::JSONReader::Read(data));
      if (message_value.get() &&
          message_value->IsType(Value::TYPE_DICTIONARY)) {
        response_dict.reset(
            static_cast<DictionaryValue*>(message_value.release()));
      }
      if (response_code == net::HTTP_INTERNAL_SERVER_ERROR) {
        request_type_ = NO_PENDING_REQUEST;
        // TODO(ahutter): Do something with the response. See
        // http://crbug.com/164410.
        observer_->OnWalletError();
        return;
      }
      break;
    }
    // Anything else is an error.
    default: {
      request_type_ = NO_PENDING_REQUEST;
      observer_->OnNetworkError(response_code);
      return;
    }
  }

  RequestType type = request_type_;
  request_type_ = NO_PENDING_REQUEST;

  switch (type) {
    case ACCEPT_LEGAL_DOCUMENTS:
      observer_->OnAcceptLegalDocuments();
      break;
    case SEND_STATUS:
      observer_->OnSendExtendedAutofillStatus();
      break;
    case ENCRYPT_OTP: {
      if (!data.empty()) {
        std::vector<std::string> splits;
        base::SplitString(data, '|', &splits);
        if (splits.size() == 2)
          observer_->OnEncryptOtp(splits[1], splits[0]);
        else
          observer_->OnNetworkError(response_code);
      } else {
        observer_->OnWalletError();
      }
      break;
    }
    case ESCROW_SENSITIVE_INFORMATION:
      if (!data.empty())
        observer_->OnDidEscrowSensitiveInformation(data);
      else
        observer_->OnWalletError();
      break;
    case GET_FULL_WALLET: {
      if (response_dict.get()) {
        scoped_ptr<FullWallet> full_wallet(
            FullWallet::CreateFullWallet(*response_dict));
        if (full_wallet.get())
          observer_->OnGetFullWallet(full_wallet.get());
        else
          observer_->OnNetworkError(response_code);
      } else {
        observer_->OnWalletError();
      }
      break;
    }
    case GET_WALLET_ITEMS: {
      if (response_dict.get()) {
        scoped_ptr<WalletItems> wallet_items(
            WalletItems::CreateWalletItems(*response_dict));
        if (wallet_items.get())
          observer_->OnGetWalletItems(wallet_items.get());
        else
          observer_->OnNetworkError(response_code);
      } else {
        observer_->OnWalletError();
      }
      break;
    }
    default: {
      NOTREACHED();
    }
  }
}

WalletClient::WalletClient(net::URLRequestContextGetter* context_getter)
    : context_getter_(context_getter),
      observer_(NULL),
      request_type_(NO_PENDING_REQUEST) {
  DCHECK(context_getter);
}

WalletClient::~WalletClient() {}

}  // namespace wallet

