// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/wallet/wallet_client.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
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

namespace autofill {
namespace wallet {

namespace {

const char kJsonMimeType[] = "application/json";
const size_t kOneTimePadLength = 6;

std::string AutocheckoutStatusToString(AutocheckoutStatus status) {
  switch (status) {
    case MISSING_FIELDMAPPING:
      return "MISSING_FIELDMAPPING";
    case MISSING_ADVANCE:
      return "MISSING_ADVANCE";
    case CANNOT_PROCEED:
      return "CANNOT_PROCEED";
    case SUCCESS:
      // SUCCESS cannot be sent to the server as it will result in a failure.
      NOTREACHED();
      return "ERROR";
  }
  NOTREACHED();
  return "NOT_POSSIBLE";
}

std::string DialogTypeToFeatureString(autofill::DialogType dialog_type) {
  switch (dialog_type) {
    case DIALOG_TYPE_REQUEST_AUTOCOMPLETE:
      return "REQUEST_AUTOCOMPLETE";
    case DIALOG_TYPE_AUTOCHECKOUT:
      return "AUTOCHECKOUT";
  }
  NOTREACHED();
  return "NOT_POSSIBLE";
}

// Gets and parses required actions from a SaveToWallet response. Returns
// false if any unknown required actions are seen and true otherwise.
void GetRequiredActionsForSaveToWallet(
    const base::DictionaryValue& dict,
    std::vector<RequiredAction>* required_actions) {
  const ListValue* required_action_list;
  if (!dict.GetList("required_action", &required_action_list))
    return;

  for (size_t i = 0; i < required_action_list->GetSize(); ++i) {
    std::string action_string;
    if (required_action_list->GetString(i, &action_string)) {
      RequiredAction action = ParseRequiredActionFromString(action_string);
      if (!ActionAppliesToSaveToWallet(action)) {
        DLOG(ERROR) << "Response from Google wallet with bad required action:"
                       " \"" << action_string << "\"";
        required_actions->clear();
        return;
      }
      required_actions->push_back(action);
    }
  }
}

// Keys for JSON communication with the Online Wallet server.
const char kAcceptedLegalDocumentKey[] = "accepted_legal_document";
const char kApiKeyKey[] = "api_key";
const char kAuthResultKey[] = "auth_result";
const char kCartKey[] = "cart";
const char kEncryptedOtpKey[] = "encrypted_otp";
const char kFeatureKey[] = "feature";
const char kGoogleTransactionIdKey[] = "google_transaction_id";
const char kInstrumentIdKey[] = "instrument_id";
const char kInstrumentKey[] = "instrument";
const char kInstrumentEscrowHandleKey[] = "instrument_escrow_handle";
const char kInstrumentPhoneNumberKey[] = "instrument_phone_number";
const char kMerchantDomainKey[] = "merchant_domain";
const char kReasonKey[] = "reason";
const char kRiskParamsKey[] = "risk_params";
const char kSelectedAddressIdKey[] = "selected_address_id";
const char kSelectedInstrumentIdKey[] = "selected_instrument_id";
const char kSessionMaterialKey[] = "session_material";
const char kShippingAddressIdKey[] = "shipping_address_id";
const char kShippingAddressKey[] = "shipping_address";
const char kSuccessKey[] = "success";
const char kUpgradedBillingAddressKey[] = "upgraded_billing_address";
const char kUpgradedInstrumentIdKey[] = "upgraded_instrument_id";

}  // namespace


WalletClient::WalletClient(net::URLRequestContextGetter* context_getter,
                           WalletClientObserver* observer)
    : context_getter_(context_getter),
      observer_(observer),
      request_type_(NO_PENDING_REQUEST),
      one_time_pad_(kOneTimePadLength),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          encryption_escrow_client_(context_getter, this)) {
  DCHECK(context_getter_);
  DCHECK(observer_);
}

WalletClient::~WalletClient() {}

void WalletClient::AcceptLegalDocuments(
    const std::vector<std::string>& document_ids,
    const std::string& google_transaction_id,
    const GURL& source_url) {
  if (HasRequestInProgress()) {
    pending_requests_.push(base::Bind(&WalletClient::AcceptLegalDocuments,
                                      base::Unretained(this),
                                      document_ids,
                                      google_transaction_id,
                                      source_url));
    return;
  }

  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = ACCEPT_LEGAL_DOCUMENTS;

  base::DictionaryValue request_dict;
  request_dict.SetString(kApiKeyKey, google_apis::GetAPIKey());
  request_dict.SetString(kGoogleTransactionIdKey, google_transaction_id);
  request_dict.SetString(kMerchantDomainKey,
                         source_url.GetWithEmptyPath().spec());
  ListValue* docs_list = new ListValue();
  for (std::vector<std::string>::const_iterator it = document_ids.begin();
       it != document_ids.end();
       ++it) {
    docs_list->AppendString(*it);
  }
  request_dict.Set(kAcceptedLegalDocumentKey, docs_list);

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetAcceptLegalDocumentsUrl(), post_body);
}

void WalletClient::AuthenticateInstrument(
    const std::string& instrument_id,
    const std::string& card_verification_number,
    const std::string& obfuscated_gaia_id) {
  if (HasRequestInProgress()) {
    pending_requests_.push(base::Bind(&WalletClient::AuthenticateInstrument,
                                      base::Unretained(this),
                                      instrument_id,
                                      card_verification_number,
                                      obfuscated_gaia_id));
    return;
  }

  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  DCHECK(pending_request_body_.empty());
  request_type_ = AUTHENTICATE_INSTRUMENT;

  pending_request_body_.SetString(kApiKeyKey, google_apis::GetAPIKey());
  pending_request_body_.SetString(kRiskParamsKey, GetRiskParams());
  pending_request_body_.SetString(kInstrumentIdKey, instrument_id);

  encryption_escrow_client_.EscrowCardVerificationNumber(
      card_verification_number, obfuscated_gaia_id);
}

void WalletClient::GetFullWallet(const std::string& instrument_id,
                                 const std::string& address_id,
                                 const GURL& source_url,
                                 const Cart& cart,
                                 const std::string& google_transaction_id,
                                 autofill::DialogType dialog_type) {
  if (HasRequestInProgress()) {
    pending_requests_.push(base::Bind(&WalletClient::GetFullWallet,
                                      base::Unretained(this),
                                      instrument_id,
                                      address_id,
                                      source_url,
                                      cart,
                                      google_transaction_id,
                                      dialog_type));
    return;
  }

  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  DCHECK(pending_request_body_.empty());
  request_type_ = GET_FULL_WALLET;

  pending_request_body_.SetString(kApiKeyKey, google_apis::GetAPIKey());
  pending_request_body_.SetString(kRiskParamsKey, GetRiskParams());
  pending_request_body_.SetString(kSelectedInstrumentIdKey, instrument_id);
  pending_request_body_.SetString(kSelectedAddressIdKey, address_id);
  pending_request_body_.SetString(kMerchantDomainKey,
                                  source_url.GetWithEmptyPath().spec());
  pending_request_body_.SetString(kGoogleTransactionIdKey,
                                  google_transaction_id);
  pending_request_body_.Set(kCartKey, cart.ToDictionary().release());
  pending_request_body_.SetString(kFeatureKey,
                                  DialogTypeToFeatureString(dialog_type));

  crypto::RandBytes(&(one_time_pad_[0]), one_time_pad_.size());
  encryption_escrow_client_.EncryptOneTimePad(one_time_pad_);
}

void WalletClient::GetWalletItems(const GURL& source_url) {
  if (HasRequestInProgress()) {
    pending_requests_.push(base::Bind(&WalletClient::GetWalletItems,
                                      base::Unretained(this),
                                      source_url));
    return;
  }

  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = GET_WALLET_ITEMS;

  base::DictionaryValue request_dict;
  request_dict.SetString(kApiKeyKey, google_apis::GetAPIKey());
  request_dict.SetString(kRiskParamsKey, GetRiskParams());
  request_dict.SetString(kMerchantDomainKey,
                         source_url.GetWithEmptyPath().spec());

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetGetWalletItemsUrl(), post_body);
}

void WalletClient::SaveAddress(const Address& shipping_address,
                               const GURL& source_url) {
  if (HasRequestInProgress()) {
    pending_requests_.push(base::Bind(&WalletClient::SaveAddress,
                                      base::Unretained(this),
                                      shipping_address,
                                      source_url));
    return;
  }

  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = SAVE_ADDRESS;

  base::DictionaryValue request_dict;
  request_dict.SetString(kApiKeyKey, google_apis::GetAPIKey());
  request_dict.SetString(kRiskParamsKey, GetRiskParams());
  request_dict.SetString(kMerchantDomainKey,
                         source_url.GetWithEmptyPath().spec());

  request_dict.Set(kShippingAddressKey,
                   shipping_address.ToDictionaryWithID().release());

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetSaveToWalletUrl(), post_body);
}

void WalletClient::SaveInstrument(
    const Instrument& instrument,
    const std::string& obfuscated_gaia_id,
    const GURL& source_url) {
  if (HasRequestInProgress()) {
    pending_requests_.push(base::Bind(&WalletClient::SaveInstrument,
                                      base::Unretained(this),
                                      instrument,
                                      obfuscated_gaia_id,
                                      source_url));
    return;
  }

  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  DCHECK(pending_request_body_.empty());
  request_type_ = SAVE_INSTRUMENT;

  pending_request_body_.SetString(kApiKeyKey, google_apis::GetAPIKey());
  pending_request_body_.SetString(kRiskParamsKey, GetRiskParams());
  pending_request_body_.SetString(kMerchantDomainKey,
                                  source_url.GetWithEmptyPath().spec());

  pending_request_body_.Set(kInstrumentKey,
                            instrument.ToDictionary().release());
  pending_request_body_.SetString(kInstrumentPhoneNumberKey,
                                  instrument.address().phone_number());

  encryption_escrow_client_.EscrowInstrumentInformation(instrument,
                                                        obfuscated_gaia_id);
}

void WalletClient::SaveInstrumentAndAddress(
    const Instrument& instrument,
    const Address& address,
    const std::string& obfuscated_gaia_id,
    const GURL& source_url) {
  if (HasRequestInProgress()) {
    pending_requests_.push(base::Bind(&WalletClient::SaveInstrumentAndAddress,
                                      base::Unretained(this),
                                      instrument,
                                      address,
                                      obfuscated_gaia_id,
                                      source_url));
    return;
  }

  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  DCHECK(pending_request_body_.empty());
  request_type_ = SAVE_INSTRUMENT_AND_ADDRESS;

  pending_request_body_.SetString(kApiKeyKey, google_apis::GetAPIKey());
  pending_request_body_.SetString(kRiskParamsKey, GetRiskParams());
  pending_request_body_.SetString(kMerchantDomainKey,
                                  source_url.GetWithEmptyPath().spec());

  pending_request_body_.Set(kInstrumentKey,
                            instrument.ToDictionary().release());
  pending_request_body_.SetString(kInstrumentPhoneNumberKey,
                              instrument.address().phone_number());

  pending_request_body_.Set(kShippingAddressKey,
                        address.ToDictionaryWithID().release());

  encryption_escrow_client_.EscrowInstrumentInformation(instrument,
                                                        obfuscated_gaia_id);
}

void WalletClient::SendAutocheckoutStatus(
    AutocheckoutStatus status,
    const GURL& source_url,
    const std::string& google_transaction_id) {
  if (HasRequestInProgress()) {
    pending_requests_.push(base::Bind(&WalletClient::SendAutocheckoutStatus,
                                      base::Unretained(this),
                                      status,
                                      source_url,
                                      google_transaction_id));
    return;
  }

  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = SEND_STATUS;

  base::DictionaryValue request_dict;
  request_dict.SetString(kApiKeyKey, google_apis::GetAPIKey());
  bool success = status == SUCCESS;
  request_dict.SetBoolean(kSuccessKey, success);
  request_dict.SetString(kMerchantDomainKey,
                         source_url.GetWithEmptyPath().spec());
  if (!success)
    request_dict.SetString(kReasonKey, AutocheckoutStatusToString(status));
  request_dict.SetString(kGoogleTransactionIdKey, google_transaction_id);

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetSendStatusUrl(), post_body);
}

void WalletClient::UpdateInstrument(
    const std::string& instrument_id,
    const Address& billing_address,
    const GURL& source_url) {
  if (HasRequestInProgress()) {
    pending_requests_.push(base::Bind(&WalletClient::UpdateInstrument,
                                      base::Unretained(this),
                                      instrument_id,
                                      billing_address,
                                      source_url));
    return;
  }

  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = UPDATE_INSTRUMENT;

  base::DictionaryValue request_dict;
  request_dict.SetString(kApiKeyKey, google_apis::GetAPIKey());
  request_dict.SetString(kRiskParamsKey, GetRiskParams());
  request_dict.SetString(kMerchantDomainKey,
                         source_url.GetWithEmptyPath().spec());

  request_dict.SetString(kUpgradedInstrumentIdKey, instrument_id);
  request_dict.SetString(kInstrumentPhoneNumberKey,
                         billing_address.phone_number());
  request_dict.Set(kUpgradedBillingAddressKey,
                   billing_address.ToDictionaryWithoutID().release());

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetSaveToWalletUrl(), post_body);
}

bool WalletClient::HasRequestInProgress() const {
  return request_.get() != NULL;
}

void WalletClient::CancelPendingRequests() {
  while (!pending_requests_.empty()) {
    pending_requests_.pop();
  }
}

void WalletClient::MakeWalletRequest(const GURL& url,
                                     const std::string& post_body) {
  DCHECK(!HasRequestInProgress());

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
  DCHECK_EQ(source, request_.get());
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

  if (!(type == ACCEPT_LEGAL_DOCUMENTS || type == SEND_STATUS) &&
      !response_dict) {
    HandleMalformedResponse();
    return;
  }

  switch (type) {
    case ACCEPT_LEGAL_DOCUMENTS:
      observer_->OnDidAcceptLegalDocuments();
      break;

    case AUTHENTICATE_INSTRUMENT: {
      std::string auth_result;
      if (response_dict->GetString(kAuthResultKey, &auth_result)) {
        std::string trimmed;
        TrimWhitespaceASCII(auth_result,
                            TRIM_ALL,
                            &trimmed);
        observer_->OnDidAuthenticateInstrument(
            LowerCaseEqualsASCII(trimmed, "success"));
      } else {
        HandleMalformedResponse();
      }
      break;
    }

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
        HandleMalformedResponse();
      }
      break;
    }

    case GET_WALLET_ITEMS: {
      scoped_ptr<WalletItems> wallet_items(
          WalletItems::CreateWalletItems(*response_dict));
      if (wallet_items)
        observer_->OnDidGetWalletItems(wallet_items.Pass());
      else
        HandleMalformedResponse();
      break;
    }

    case SAVE_ADDRESS: {
      std::string shipping_address_id;
      std::vector<RequiredAction> required_actions;
      GetRequiredActionsForSaveToWallet(*response_dict, &required_actions);
      if (response_dict->GetString(kShippingAddressIdKey,
                                   &shipping_address_id) ||
          !required_actions.empty()) {
        observer_->OnDidSaveAddress(shipping_address_id, required_actions);
      } else {
        HandleMalformedResponse();
      }
      break;
    }

    case SAVE_INSTRUMENT: {
      std::string instrument_id;
      std::vector<RequiredAction> required_actions;
      GetRequiredActionsForSaveToWallet(*response_dict, &required_actions);
      if (response_dict->GetString(kInstrumentIdKey, &instrument_id) ||
          !required_actions.empty()) {
        observer_->OnDidSaveInstrument(instrument_id, required_actions);
      } else {
        HandleMalformedResponse();
      }
      break;
    }

    case SAVE_INSTRUMENT_AND_ADDRESS: {
      std::string instrument_id;
      response_dict->GetString(kInstrumentIdKey, &instrument_id);
      std::string shipping_address_id;
      response_dict->GetString(kShippingAddressIdKey,
                               &shipping_address_id);
      std::vector<RequiredAction> required_actions;
      GetRequiredActionsForSaveToWallet(*response_dict, &required_actions);
      if ((!instrument_id.empty() && !shipping_address_id.empty()) ||
          !required_actions.empty()) {
        observer_->OnDidSaveInstrumentAndAddress(instrument_id,
                                                 shipping_address_id,
                                                 required_actions);
      } else {
        HandleMalformedResponse();
      }
      break;
    }

    case UPDATE_INSTRUMENT: {
      std::string instrument_id;
      std::vector<RequiredAction> required_actions;
      GetRequiredActionsForSaveToWallet(*response_dict, &required_actions);
      if (response_dict->GetString(kInstrumentIdKey, &instrument_id) ||
          !required_actions.empty()) {
        observer_->OnDidUpdateInstrument(instrument_id, required_actions);
      } else {
        HandleMalformedResponse();
      }
      break;
    }

    case NO_PENDING_REQUEST:
      NOTREACHED();
  }

  request_.reset();
  StartNextPendingRequest();
}

void WalletClient::StartNextPendingRequest() {
  if (pending_requests_.empty())
    return;

  base::Closure next_request = pending_requests_.front();
  pending_requests_.pop();
  next_request.Run();
}

void WalletClient::HandleMalformedResponse() {
  // Called to inform exponential backoff logic of the error.
  request_->ReceivedContentWasMalformed();
  observer_->OnMalformedResponse();
}

void WalletClient::OnDidEncryptOneTimePad(
    const std::string& encrypted_one_time_pad,
    const std::string& session_material) {
  DCHECK_EQ(GET_FULL_WALLET, request_type_);
  pending_request_body_.SetString(kEncryptedOtpKey, encrypted_one_time_pad);
  pending_request_body_.SetString(kSessionMaterialKey, session_material);

  std::string post_body;
  base::JSONWriter::Write(&pending_request_body_, &post_body);
  pending_request_body_.Clear();

  MakeWalletRequest(GetGetFullWalletUrl(), post_body);
}

void WalletClient::OnDidEscrowInstrumentInformation(
    const std::string& escrow_handle) {
  DCHECK(request_type_ == SAVE_INSTRUMENT ||
         request_type_ == SAVE_INSTRUMENT_AND_ADDRESS);

  pending_request_body_.SetString(kInstrumentEscrowHandleKey, escrow_handle);

  std::string post_body;
  base::JSONWriter::Write(&pending_request_body_, &post_body);
  pending_request_body_.Clear();

  MakeWalletRequest(GetSaveToWalletUrl(), post_body);
}

void WalletClient::OnDidEscrowCardVerificationNumber(
    const std::string& escrow_handle) {
  DCHECK_EQ(AUTHENTICATE_INSTRUMENT, request_type_);
  pending_request_body_.SetString(kInstrumentEscrowHandleKey, escrow_handle);

  std::string post_body;
  base::JSONWriter::Write(&pending_request_body_, &post_body);
  pending_request_body_.Clear();

  MakeWalletRequest(GetAuthenticateInstrumentUrl(), post_body);
}

void WalletClient::OnNetworkError(int response_code) {
  observer_->OnNetworkError(response_code);
}

void WalletClient::OnMalformedResponse() {
  observer_->OnMalformedResponse();
}

}  // namespace wallet
}  // namespace autofill
