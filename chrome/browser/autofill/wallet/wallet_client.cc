// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/wallet/wallet_client.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autofill/wallet/cart.h"
#include "chrome/browser/autofill/wallet/instrument.h"
#include "chrome/browser/autofill/wallet/wallet_address.h"
#include "chrome/browser/autofill/wallet/wallet_client_observer.h"
#include "chrome/browser/autofill/wallet/wallet_items.h"
#include "chrome/browser/autofill/wallet/wallet_service_url.h"
#include "crypto/random.h"
#include "google_apis/google_api_keys.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

const char kJsonMimeType[] = "application/json";
const size_t kOneTimePadLength = 6;

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

namespace autofill {
namespace wallet {

WalletClient::WalletClient(net::URLRequestContextGetter* context_getter)
    : context_getter_(context_getter),
      observer_(NULL),
      request_type_(NO_PENDING_REQUEST),
      one_time_pad_(kOneTimePadLength),
      encryption_escrow_client_(context_getter),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  DCHECK(context_getter);
}

WalletClient::~WalletClient() {}

void WalletClient::AcceptLegalDocuments(
    const std::vector<std::string>& document_ids,
    const std::string& google_transaction_id,
    WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = ACCEPT_LEGAL_DOCUMENTS;

  base::DictionaryValue request_dict;
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

  MakeWalletRequest(GetAcceptLegalDocumentsUrl(), post_body, observer);
}

void WalletClient::GetFullWallet(const std::string& instrument_id,
                                 const std::string& address_id,
                                 const std::string& merchant_domain,
                                 const Cart& cart,
                                 const std::string& google_transaction_id,
                                 WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  DCHECK(observer);
  DCHECK(pending_request_body_.empty());
  request_type_ = GET_FULL_WALLET;
  observer_ = observer;

  pending_request_body_.SetString("api_key", google_apis::GetAPIKey());
  pending_request_body_.SetString("risk_params", GetRiskParams());
  pending_request_body_.SetString("selected_instrument_id", instrument_id);
  pending_request_body_.SetString("selected_address_id", address_id);
  pending_request_body_.SetString("merchant_domain", merchant_domain);
  pending_request_body_.SetString("google_transaction_id",
                                  google_transaction_id);
  pending_request_body_.Set("cart", cart.ToDictionary().release());

  crypto::RandBytes(&(one_time_pad_[0]), one_time_pad_.size());
  encryption_escrow_client_.EncryptOneTimePad(one_time_pad_,
                                              weak_ptr_factory_.GetWeakPtr());
}

void WalletClient::GetWalletItems(WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = GET_WALLET_ITEMS;

  base::DictionaryValue request_dict;
  request_dict.SetString("api_key", google_apis::GetAPIKey());
  request_dict.SetString("risk_params", GetRiskParams());

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetGetWalletItemsUrl(), post_body, observer);
}

void WalletClient::SaveAddress(const Address& shipping_address,
                               WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = SAVE_ADDRESS;

  base::DictionaryValue request_dict;
  request_dict.SetString("api_key", google_apis::GetAPIKey());
  request_dict.SetString("risk_params", GetRiskParams());

  request_dict.Set("shipping_address",
                   shipping_address.ToDictionaryWithID().release());

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetSaveToWalletUrl(), post_body, observer);
}

void WalletClient::SaveInstrument(const Instrument& instrument,
                                  const std::string& obfuscated_gaia_id,
                                  WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  DCHECK(observer);
  DCHECK(pending_request_body_.empty());
  request_type_ = SAVE_INSTRUMENT;
  observer_ = observer;

  pending_request_body_.SetString("api_key", google_apis::GetAPIKey());
  pending_request_body_.SetString("risk_params", GetRiskParams());

  pending_request_body_.Set("instrument", instrument.ToDictionary().release());
  pending_request_body_.SetString("instrument_phone_number",
                                  instrument.address().phone_number());

  encryption_escrow_client_.EscrowSensitiveInformation(
      instrument,
      obfuscated_gaia_id,
      weak_ptr_factory_.GetWeakPtr());
}

void WalletClient::SaveInstrumentAndAddress(
    const Instrument& instrument,
    const Address& address,
    const std::string& obfuscated_gaia_id,
    WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  DCHECK(observer);
  DCHECK(pending_request_body_.empty());
  request_type_ = SAVE_INSTRUMENT_AND_ADDRESS;
  observer_ = observer;

  pending_request_body_.SetString("api_key", google_apis::GetAPIKey());
  pending_request_body_.SetString("risk_params", GetRiskParams());

  pending_request_body_.Set("instrument", instrument.ToDictionary().release());
  pending_request_body_.SetString("instrument_phone_number",
                              instrument.address().phone_number());

  pending_request_body_.Set("shipping_address",
                        address.ToDictionaryWithID().release());

  encryption_escrow_client_.EscrowSensitiveInformation(
      instrument,
      obfuscated_gaia_id,
      weak_ptr_factory_.GetWeakPtr());
}

void WalletClient::SendAutocheckoutStatus(
    autofill::AutocheckoutStatus status,
    const std::string& merchant_domain,
    const std::string& google_transaction_id,
    WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = SEND_STATUS;

  base::DictionaryValue request_dict;
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

  MakeWalletRequest(GetSendStatusUrl(), post_body, observer);
}

void WalletClient::UpdateInstrument(const std::string& instrument_id,
                                    const Address& billing_address,
                                    WalletClientObserver* observer) {
  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = UPDATE_INSTRUMENT;

  base::DictionaryValue request_dict;
  request_dict.SetString("api_key", google_apis::GetAPIKey());
  request_dict.SetString("upgraded_instrument_id", instrument_id);
  request_dict.SetString("instrument_phone_number",
                         billing_address.phone_number());
  request_dict.Set("upgraded_billing_address",
                   billing_address.ToDictionaryWithoutID().release());

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetSaveToWalletUrl(), post_body, observer);
}

bool WalletClient::HasRequestInProgress() const {
  return request_.get() != NULL;
}

void WalletClient::MakeWalletRequest(const GURL& url,
                                     const std::string& post_body,
                                     WalletClientObserver* observer) {
  DCHECK(!HasRequestInProgress());
  DCHECK(observer);

  observer_ = observer;

  request_.reset(net::URLFetcher::Create(
      0, url, net::URLFetcher::POST, this));
  request_->SetRequestContext(context_getter_);
  DVLOG(1) << "url=" << url << ", post_body=" << post_body;
  request_->SetUploadData(kJsonMimeType, post_body);
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

  scoped_ptr<base::DictionaryValue> response_dict;

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
            static_cast<base::DictionaryValue*>(message_value.release()));
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

  if (!(type == ACCEPT_LEGAL_DOCUMENTS || SEND_STATUS) && !response_dict) {
    HandleMalformedResponse(old_request.get());
    return;
  }

  switch (type) {
    case ACCEPT_LEGAL_DOCUMENTS:
      observer_->OnDidAcceptLegalDocuments();
      break;
    case SEND_STATUS:
      observer_->OnDidSendAutocheckoutStatus();
      break;
    case GET_FULL_WALLET: {
      scoped_ptr<FullWallet> full_wallet(
          FullWallet::CreateFullWallet(*response_dict));
      if (full_wallet) {
        full_wallet->set_one_time_pad(one_time_pad_);
        observer_->OnDidGetFullWallet(full_wallet.Pass());
      } else {
        HandleMalformedResponse(old_request.get());
      }
      break;
    }
    case GET_WALLET_ITEMS: {
      scoped_ptr<WalletItems> wallet_items(
          WalletItems::CreateWalletItems(*response_dict));
      if (wallet_items)
        observer_->OnDidGetWalletItems(wallet_items.Pass());
      else
        HandleMalformedResponse(old_request.get());
      break;
    }
    case SAVE_ADDRESS: {
      std::string shipping_address_id;
      if (response_dict->GetString("shipping_address_id",
                                   &shipping_address_id))
        observer_->OnDidSaveAddress(shipping_address_id);
      else
        HandleMalformedResponse(old_request.get());
      break;
    }
    case SAVE_INSTRUMENT: {
      std::string instrument_id;
      if (response_dict->GetString("instrument_id", &instrument_id))
        observer_->OnDidSaveInstrument(instrument_id);
      else
        HandleMalformedResponse(old_request.get());
      break;
    }
    case SAVE_INSTRUMENT_AND_ADDRESS: {
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
      break;
    }
    case UPDATE_INSTRUMENT: {
      std::string instrument_id;
      response_dict->GetString("instrument_id", &instrument_id);
      if (!instrument_id.empty())
        observer_->OnDidUpdateInstrument(instrument_id);
      else
        HandleMalformedResponse(old_request.get());
      break;
    }
    case NO_PENDING_REQUEST:
      NOTREACHED();
  }
}

void WalletClient::HandleMalformedResponse(net::URLFetcher* request) {
  // Called to inform exponential backoff logic of the error.
  request->ReceivedContentWasMalformed();
  observer_->OnMalformedResponse();
}

void WalletClient::OnDidEncryptOneTimePad(
    const std::string& encrypted_one_time_pad,
    const std::string& session_material) {
  DCHECK_EQ(GET_FULL_WALLET, request_type_);
  pending_request_body_.SetString("encrypted_otp", encrypted_one_time_pad);
  pending_request_body_.SetString("session_material", session_material);

  std::string post_body;
  base::JSONWriter::Write(&pending_request_body_, &post_body);
  pending_request_body_.Clear();

  MakeWalletRequest(GetGetFullWalletUrl(), post_body, observer_);
}

void WalletClient::OnDidEscrowSensitiveInformation(
    const std::string& escrow_handle) {
  DCHECK(request_type_ == SAVE_INSTRUMENT ||
         request_type_ == SAVE_INSTRUMENT_AND_ADDRESS);

  pending_request_body_.SetString("instrument_escrow_handle", escrow_handle);

  std::string post_body;
  base::JSONWriter::Write(&pending_request_body_, &post_body);
  pending_request_body_.Clear();

  MakeWalletRequest(GetSaveToWalletUrl(), post_body, observer_);
}

void WalletClient::OnNetworkError(int response_code) {
  observer_->OnNetworkError(response_code);
}

void WalletClient::OnMalformedResponse() {
  observer_->OnMalformedResponse();
}

}  // namespace wallet
}  // namespace autofill
