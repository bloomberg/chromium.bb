// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/wallet/wallet_client.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill/wallet/cart.h"
#include "chrome/browser/autofill/wallet/full_wallet.h"
#include "chrome/browser/autofill/wallet/instrument.h"
#include "chrome/browser/autofill/wallet/wallet_address.h"
#include "chrome/browser/autofill/wallet/wallet_client_observer.h"
#include "chrome/browser/autofill/wallet/wallet_items.h"
#include "chrome/browser/autofill/wallet/wallet_service_url.h"
#include "google_apis/google_api_keys.h"
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

std::string AutocheckoutStatusToString(autofill::AutocheckoutStatus status) {
  switch (status) {
    case autofill::MISSING_FIELDMAPPING:
      return "MISSING_FIELDMAPPING";
    case autofill::MISSING_ADVANCE:
      return "MISSING_ADVANCE";
    case autofill::CANNOT_PROCEED:
      return "CANNOT_PROCEED";
    case autofill::SUCCESS:
      // SUCCESS cannot be sent to the server as it will result in a failure.
      NOTREACHED();
      return "ERROR";
  }
  NOTREACHED();
  return "NOT_POSSIBLE";
}

}  // anonymous namespace

namespace wallet {

void WalletClient::AcceptLegalDocuments(
    const std::vector<std::string>& document_ids,
    const std::string& google_transaction_id,
    WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = ACCEPT_LEGAL_DOCUMENTS;

  DictionaryValue request_dict;
  request_dict.SetString("api_key", google_apis::GetAPIKey());
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

void WalletClient::EncryptOtp(const void* otp,
                              size_t length,
                              WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  size_t num_bits = length * 8;
  DCHECK_LT(num_bits, kMaxBits);

  request_type_ = ENCRYPT_OTP;

  std::string post_body = base::StringPrintf(
      kEncryptOtpBodyFormat,
      base::HexEncode(&num_bits, 1).c_str(),
      base::HexEncode(otp, length).c_str());

  MakeWalletRequest(GetEncryptionUrl(),
                    post_body,
                    observer,
                    kApplicationMimeType);
}

void WalletClient::EscrowSensitiveInformation(
    const Instrument& new_instrument,
    const std::string& obfuscated_gaia_id,
    WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = ESCROW_SENSITIVE_INFORMATION;

  std::string post_body = base::StringPrintf(
      kEscrowSensitiveInformationFormat,
      obfuscated_gaia_id.c_str(),
      UTF16ToUTF8(new_instrument.primary_account_number()).c_str(),
      UTF16ToUTF8(new_instrument.card_verification_number()).c_str());

  MakeWalletRequest(GetEscrowUrl(), post_body, observer, kApplicationMimeType);
}


void WalletClient::GetFullWallet(const std::string& instrument_id,
                                 const std::string& address_id,
                                 const std::string& merchant_domain,
                                 const Cart& cart,
                                 const std::string& google_transaction_id,
                                 const std::string& encrypted_otp,
                                 const std::string& session_material,
                                 WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = GET_FULL_WALLET;

  DictionaryValue request_dict;
  request_dict.SetString("api_key", google_apis::GetAPIKey());
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

void WalletClient::GetWalletItems(WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = GET_WALLET_ITEMS;

  DictionaryValue request_dict;
  request_dict.SetString("api_key", google_apis::GetAPIKey());
  request_dict.SetString("risk_params", GetRiskParams());

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetGetWalletItemsUrl(), post_body, observer, kJsonMimeType);
}

void WalletClient::SaveAddress(const Address& shipping_address,
                               WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = SAVE_ADDRESS;

  SaveToWallet(NULL,
               std::string(),
               &shipping_address,
               observer);
}

void WalletClient::SaveInstrument(const Instrument& instrument,
                                  const std::string& escrow_handle,
                                  WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = SAVE_INSTRUMENT;

  SaveToWallet(&instrument,
               escrow_handle,
               NULL,
               observer);
}

void WalletClient::SaveInstrumentAndAddress(const Instrument& instrument,
                                            const std::string& escrow_handle,
                                            const Address& address,
                                            WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = SAVE_INSTRUMENT_AND_ADDRESS;

  SaveToWallet(&instrument,
               escrow_handle,
               &address,
               observer);
}

void WalletClient::SaveToWallet(const Instrument* instrument,
                                const std::string& escrow_handle,
                                const Address* shipping_address,
                                WalletClientObserver* observer) {
  DictionaryValue request_dict;
  request_dict.SetString("api_key", google_apis::GetAPIKey());
  request_dict.SetString("risk_params", GetRiskParams());

  if (instrument) {
    request_dict.Set("instrument", instrument->ToDictionary().release());
    request_dict.SetString("instrument_escrow_handle", escrow_handle);
    request_dict.SetString("instrument_phone_number",
                           instrument->address().phone_number());
  }

  if (shipping_address) {
    request_dict.Set("shipping_address",
                     shipping_address->ToDictionaryWithID().release());
  }

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetSaveToWalletUrl(), post_body, observer, kJsonMimeType);
}

void WalletClient::SendAutocheckoutStatus(
    autofill::AutocheckoutStatus status,
    const std::string& merchant_domain,
    const std::string& google_transaction_id,
    WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = SEND_STATUS;

  DictionaryValue request_dict;
  request_dict.SetString("api_key", google_apis::GetAPIKey());
  bool success = status == autofill::SUCCESS;
  request_dict.SetBoolean("success", success);
  request_dict.SetString("hostname", merchant_domain);
  if (!success) {
    request_dict.SetString("reason", AutocheckoutStatusToString(status));
  }
  request_dict.SetString("google_transaction_id", google_transaction_id);

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetSendStatusUrl(), post_body, observer, kJsonMimeType);
}

void WalletClient::UpdateInstrument(const std::string& instrument_id,
                                    const Address& billing_address,
                                    WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = UPDATE_INSTRUMENT;

  DictionaryValue request_dict;
  request_dict.SetString("api_key", google_apis::GetAPIKey());
  request_dict.SetString("upgraded_instrument_id", instrument_id);
  request_dict.SetString("instrument_phone_number",
                         billing_address.phone_number());
  request_dict.Set("upgraded_billing_address",
                   billing_address.ToDictionaryWithoutID().release());

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetSaveToWalletUrl(), post_body, observer, kJsonMimeType);
}

bool WalletClient::HasRequestInProgress() const {
  return request_.get() != NULL;
}

void WalletClient::MakeWalletRequest(const GURL& url,
                                     const std::string& post_body,
                                     WalletClientObserver* observer,
                                     const std::string& content_type) {
  DCHECK(!HasRequestInProgress());
  DCHECK(observer);

  observer_ = observer;

  request_.reset(net::URLFetcher::Create(
      0, url, net::URLFetcher::POST, this));
  request_->SetRequestContext(context_getter_);
  DVLOG(1) << "url=" << url << ", post_body=" << post_body
           << ", content_type=" << content_type;
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
  source->GetResponseAsString(&data);
  DVLOG(1) << "Response body: " << data;

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
      observer_->OnDidAcceptLegalDocuments();
      break;
    case SEND_STATUS:
      observer_->OnDidSendAutocheckoutStatus();
      break;
    case ENCRYPT_OTP:
      if (!data.empty()) {
        std::vector<std::string> splits;
        base::SplitString(data, '|', &splits);
        if (splits.size() == 2)
          observer_->OnDidEncryptOtp(splits[1], splits[0]);
        else
          HandleMalformedResponse(old_request.get());
      } else {
        HandleMalformedResponse(old_request.get());
      }
      break;
    case ESCROW_SENSITIVE_INFORMATION:
      if (!data.empty())
        observer_->OnDidEscrowSensitiveInformation(data);
      else
        HandleMalformedResponse(old_request.get());
      break;
    case GET_FULL_WALLET:
      if (response_dict) {
        scoped_ptr<FullWallet> full_wallet(
            FullWallet::CreateFullWallet(*response_dict));
        if (full_wallet)
          observer_->OnDidGetFullWallet(full_wallet.Pass());
        else
          HandleMalformedResponse(old_request.get());
      } else {
        HandleMalformedResponse(old_request.get());
      }
      break;
    case GET_WALLET_ITEMS:
      if (response_dict) {
        scoped_ptr<WalletItems> wallet_items(
            WalletItems::CreateWalletItems(*response_dict));
        if (wallet_items)
          observer_->OnDidGetWalletItems(wallet_items.Pass());
        else
          HandleMalformedResponse(old_request.get());
      } else {
        HandleMalformedResponse(old_request.get());
      }
      break;
    case SAVE_ADDRESS:
      if (response_dict) {
        std::string shipping_address_id;
        if (response_dict->GetString("shipping_address_id",
                                     &shipping_address_id))
          observer_->OnDidSaveAddress(shipping_address_id);
        else
          HandleMalformedResponse(old_request.get());
      } else {
        HandleMalformedResponse(old_request.get());
      }
      break;
    case SAVE_INSTRUMENT:
      if (response_dict) {
        std::string instrument_id;
        if (response_dict->GetString("instrument_id", &instrument_id))
          observer_->OnDidSaveInstrument(instrument_id);
        else
          HandleMalformedResponse(old_request.get());
      } else {
        HandleMalformedResponse(old_request.get());
      }
      break;
    case SAVE_INSTRUMENT_AND_ADDRESS:
      if (response_dict) {
        std::string instrument_id;
        response_dict->GetString("instrument_id", &instrument_id);
        std::string shipping_address_id;
        response_dict->GetString("shipping_address_id",
                                 &shipping_address_id);
        if (!instrument_id.empty() && !shipping_address_id.empty()) {
          observer_->OnDidSaveInstrumentAndAddress(instrument_id,
                                                   shipping_address_id);
        } else {
          HandleMalformedResponse(old_request.get());
        }
      } else {
        HandleMalformedResponse(old_request.get());
      }
      break;
    case UPDATE_INSTRUMENT:
      if (response_dict) {
        std::string instrument_id;
        response_dict->GetString("instrument_id", &instrument_id);
        if (!instrument_id.empty())
          observer_->OnDidUpdateInstrument(instrument_id);
        else
          HandleMalformedResponse(old_request.get());
      } else {
        HandleMalformedResponse(old_request.get());
      }
      break;
    case NO_PENDING_REQUEST:
      NOTREACHED();
  }
}

void WalletClient::HandleMalformedResponse(net::URLFetcher* request) {
  // Called to inform exponential backoff logic of the error.
  request->ReceivedContentWasMalformed();
  observer_->OnMalformedResponse();
}

WalletClient::WalletClient(net::URLRequestContextGetter* context_getter)
    : context_getter_(context_getter),
      observer_(NULL),
      request_type_(NO_PENDING_REQUEST) {
  DCHECK(context_getter);
}

WalletClient::~WalletClient() {}

}  // namespace wallet

