// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/wallet/wallet_client.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "components/autofill/browser/autofill_metrics.h"
#include "components/autofill/browser/wallet/instrument.h"
#include "components/autofill/browser/wallet/wallet_address.h"
#include "components/autofill/browser/wallet/wallet_client_delegate.h"
#include "components/autofill/browser/wallet/wallet_items.h"
#include "components/autofill/browser/wallet/wallet_service_url.h"
#include "crypto/random.h"
#include "google_apis/google_api_keys.h"
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

std::string RiskCapabilityToString(
    WalletClient::RiskCapability risk_capability) {
  switch (risk_capability) {
    case WalletClient::RELOGIN:
      return "RELOGIN";
    case WalletClient::VERIFY_CVC:
      return "VERIFY_CVC";
  }
  NOTREACHED();
  return "NOT_POSSIBLE";
}

WalletClient::ErrorType StringToErrorType(const std::string& error_type) {
  std::string trimmed;
  TrimWhitespaceASCII(error_type,
                      TRIM_ALL,
                      &trimmed);
  if (LowerCaseEqualsASCII(trimmed, "buyer_account_error"))
    return WalletClient::BUYER_ACCOUNT_ERROR;
  if (LowerCaseEqualsASCII(trimmed, "internal_error"))
    return WalletClient::INTERNAL_ERROR;
  if (LowerCaseEqualsASCII(trimmed, "invalid_params"))
    return WalletClient::INVALID_PARAMS;
  if (LowerCaseEqualsASCII(trimmed, "service_unavailable"))
    return WalletClient::SERVICE_UNAVAILABLE;
  if (LowerCaseEqualsASCII(trimmed, "spending_limit_exceeded"))
    return WalletClient::SPENDING_LIMIT_EXCEEDED;
  if (LowerCaseEqualsASCII(trimmed, "unsupported_api_version"))
    return WalletClient::UNSUPPORTED_API_VERSION;
  return WalletClient::UNKNOWN_ERROR;
}

// Gets and parses required actions from a SaveToWallet response. Returns
// false if any unknown required actions are seen and true otherwise.
void GetRequiredActionsForSaveToWallet(
    const base::DictionaryValue& dict,
    std::vector<RequiredAction>* required_actions) {
  const base::ListValue* required_action_list;
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

// Converts the |error_type| to the corresponding value from the stable UMA
// metric enumeration.
AutofillMetrics::WalletErrorMetric ErrorTypeToUmaMetric(
    WalletClient::ErrorType error_type) {
  switch (error_type) {
    case WalletClient::BAD_REQUEST:
      return AutofillMetrics::WALLET_BAD_REQUEST;
    case WalletClient::BUYER_ACCOUNT_ERROR:
      return AutofillMetrics::WALLET_BUYER_ACCOUNT_ERROR;
    case WalletClient::INTERNAL_ERROR:
      return AutofillMetrics::WALLET_INTERNAL_ERROR;
    case WalletClient::INVALID_PARAMS:
      return AutofillMetrics::WALLET_INVALID_PARAMS;
    case WalletClient::SERVICE_UNAVAILABLE:
      return AutofillMetrics::WALLET_SERVICE_UNAVAILABLE;
    case WalletClient::SPENDING_LIMIT_EXCEEDED:
      return AutofillMetrics::WALLET_SPENDING_LIMIT_EXCEEDED;
    case WalletClient::UNSUPPORTED_API_VERSION:
      return AutofillMetrics::WALLET_UNSUPPORTED_API_VERSION;
    case WalletClient::UNKNOWN_ERROR:
      return AutofillMetrics::WALLET_UNKNOWN_ERROR;
  }

  NOTREACHED();
  return AutofillMetrics::WALLET_UNKNOWN_ERROR;
}

// Converts the |required_action| to the corresponding value from the stable UMA
// metric enumeration.
AutofillMetrics::WalletRequiredActionMetric RequiredActionToUmaMetric(
    RequiredAction required_action) {
  switch (required_action) {
    case UNKNOWN_TYPE:
      return AutofillMetrics::UNKNOWN_TYPE;
    case CHOOSE_ANOTHER_INSTRUMENT_OR_ADDRESS:
      return AutofillMetrics::CHOOSE_ANOTHER_INSTRUMENT_OR_ADDRESS;
    case SETUP_WALLET:
      return AutofillMetrics::SETUP_WALLET;
    case ACCEPT_TOS:
      return AutofillMetrics::ACCEPT_TOS;
    case GAIA_AUTH:
      return AutofillMetrics::GAIA_AUTH;
    case UPDATE_EXPIRATION_DATE:
      return AutofillMetrics::UPDATE_EXPIRATION_DATE;
    case UPGRADE_MIN_ADDRESS:
      return AutofillMetrics::UPGRADE_MIN_ADDRESS;
    case INVALID_FORM_FIELD:
      return AutofillMetrics::INVALID_FORM_FIELD;
    case VERIFY_CVV:
      return AutofillMetrics::VERIFY_CVV;
    case PASSIVE_GAIA_AUTH:
      return AutofillMetrics::PASSIVE_GAIA_AUTH;
    case REQUIRE_PHONE_NUMBER:
      return AutofillMetrics::REQUIRE_PHONE_NUMBER;
  }

  NOTREACHED();
  return AutofillMetrics::UNKNOWN_TYPE;
}

// Keys for JSON communication with the Online Wallet server.
const char kAcceptedLegalDocumentKey[] = "accepted_legal_document";
const char kApiKeyKey[] = "api_key";
const char kAuthResultKey[] = "auth_result";
const char kCartKey[] = "cart";
const char kEncryptedOtpKey[] = "encrypted_otp";
const char kErrorTypeKey[] = "wallet_error.error_type";
const char kFeatureKey[] = "feature";
const char kGoogleTransactionIdKey[] = "google_transaction_id";
const char kInstrumentIdKey[] = "instrument_id";
const char kInstrumentKey[] = "instrument";
const char kInstrumentEscrowHandleKey[] = "instrument_escrow_handle";
const char kInstrumentPhoneNumberKey[] = "instrument_phone_number";
const char kMerchantDomainKey[] = "merchant_domain";
const char kReasonKey[] = "reason";
const char kRiskCapabilitiesKey[] = "supported_risk_challenge";
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

WalletClient::FullWalletRequest::FullWalletRequest(
    const std::string& instrument_id,
    const std::string& address_id,
    const GURL& source_url,
    const Cart& cart,
    const std::string& google_transaction_id,
    const std::vector<RiskCapability> risk_capabilities)
    : instrument_id(instrument_id),
      address_id(address_id),
      source_url(source_url),
      cart(cart),
      google_transaction_id(google_transaction_id),
      risk_capabilities(risk_capabilities) {}

WalletClient::FullWalletRequest::~FullWalletRequest() {}

WalletClient::WalletClient(net::URLRequestContextGetter* context_getter,
                           WalletClientDelegate* delegate)
    : context_getter_(context_getter),
      delegate_(delegate),
      request_type_(NO_PENDING_REQUEST),
      one_time_pad_(kOneTimePadLength),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          encryption_escrow_client_(context_getter, this)) {
  DCHECK(context_getter_);
  DCHECK(delegate_);
}

WalletClient::~WalletClient() {}

void WalletClient::AcceptLegalDocuments(
    const std::vector<WalletItems::LegalDocument*>& documents,
    const std::string& google_transaction_id,
    const GURL& source_url) {
  if (documents.empty())
    return;

  std::vector<std::string> document_ids;
  for (size_t i = 0; i < documents.size(); ++i) {
    document_ids.push_back(documents[i]->id());
  }
  DoAcceptLegalDocuments(document_ids, google_transaction_id, source_url);
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
  pending_request_body_.SetString(kRiskParamsKey, delegate_->GetRiskData());
  pending_request_body_.SetString(kInstrumentIdKey, instrument_id);

  encryption_escrow_client_.EscrowCardVerificationNumber(
      card_verification_number, obfuscated_gaia_id);
}

void WalletClient::GetFullWallet(const FullWalletRequest& full_wallet_request) {
  if (HasRequestInProgress()) {
    pending_requests_.push(base::Bind(&WalletClient::GetFullWallet,
                                      base::Unretained(this),
                                      full_wallet_request));
    return;
  }

  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  DCHECK(pending_request_body_.empty());
  request_type_ = GET_FULL_WALLET;

  pending_request_body_.SetString(kApiKeyKey, google_apis::GetAPIKey());
  pending_request_body_.SetString(kRiskParamsKey, delegate_->GetRiskData());
  pending_request_body_.SetString(kSelectedInstrumentIdKey,
                                  full_wallet_request.instrument_id);
  pending_request_body_.SetString(kSelectedAddressIdKey,
                                  full_wallet_request.address_id);
  pending_request_body_.SetString(
      kMerchantDomainKey,
      full_wallet_request.source_url.GetWithEmptyPath().spec());
  pending_request_body_.SetString(kGoogleTransactionIdKey,
                                  full_wallet_request.google_transaction_id);
  pending_request_body_.Set(kCartKey,
                            full_wallet_request.cart.ToDictionary().release());
  pending_request_body_.SetString(
      kFeatureKey,
      DialogTypeToFeatureString(delegate_->GetDialogType()));

  scoped_ptr<base::ListValue> risk_capabilities_list(new base::ListValue());
  for (std::vector<RiskCapability>::const_iterator it =
           full_wallet_request.risk_capabilities.begin();
       it != full_wallet_request.risk_capabilities.end();
       ++it) {
    risk_capabilities_list->AppendString(RiskCapabilityToString(*it));
  }
  pending_request_body_.Set(kRiskCapabilitiesKey,
                            risk_capabilities_list.release());

  crypto::RandBytes(&(one_time_pad_[0]), one_time_pad_.size());
  encryption_escrow_client_.EncryptOneTimePad(one_time_pad_);
}

void WalletClient::GetWalletItems(
    const GURL& source_url,
    const std::vector<RiskCapability>& risk_capabilities) {
  if (HasRequestInProgress()) {
    pending_requests_.push(base::Bind(&WalletClient::GetWalletItems,
                                      base::Unretained(this),
                                      source_url,
                                      risk_capabilities));
    return;
  }

  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = GET_WALLET_ITEMS;

  base::DictionaryValue request_dict;
  request_dict.SetString(kApiKeyKey, google_apis::GetAPIKey());
  request_dict.SetString(kRiskParamsKey, delegate_->GetRiskData());
  request_dict.SetString(kMerchantDomainKey,
                         source_url.GetWithEmptyPath().spec());

  scoped_ptr<base::ListValue> risk_capabilities_list(new base::ListValue());
  for (std::vector<RiskCapability>::const_iterator it =
           risk_capabilities.begin();
       it != risk_capabilities.end();
       ++it) {
    risk_capabilities_list->AppendString(RiskCapabilityToString(*it));
  }
  request_dict.Set(kRiskCapabilitiesKey, risk_capabilities_list.release());

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
  request_dict.SetString(kRiskParamsKey, delegate_->GetRiskData());
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
  pending_request_body_.SetString(kRiskParamsKey, delegate_->GetRiskData());
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
  pending_request_body_.SetString(kRiskParamsKey, delegate_->GetRiskData());
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

void WalletClient::UpdateAddress(const Address& address,
                                 const GURL& source_url) {
  if (HasRequestInProgress()) {
    pending_requests_.push(base::Bind(&WalletClient::UpdateAddress,
                                      base::Unretained(this),
                                      address,
                                      source_url));
    return;
  }

  DCHECK_EQ(NO_PENDING_REQUEST, request_type_);
  request_type_ = UPDATE_ADDRESS;

  base::DictionaryValue request_dict;
  request_dict.SetString(kApiKeyKey, google_apis::GetAPIKey());
  request_dict.SetString(kRiskParamsKey, delegate_->GetRiskData());
  request_dict.SetString(kMerchantDomainKey,
                         source_url.GetWithEmptyPath().spec());

  request_dict.Set(kShippingAddressKey,
                   address.ToDictionaryWithID().release());

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetSaveToWalletUrl(), post_body);
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
  request_dict.SetString(kRiskParamsKey, delegate_->GetRiskData());
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

void WalletClient::DoAcceptLegalDocuments(
    const std::vector<std::string>& document_ids,
    const std::string& google_transaction_id,
    const GURL& source_url) {
  if (HasRequestInProgress()) {
    pending_requests_.push(base::Bind(&WalletClient::DoAcceptLegalDocuments,
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
  scoped_ptr<base::ListValue> docs_list(new base::ListValue());
  for (std::vector<std::string>::const_iterator it = document_ids.begin();
       it != document_ids.end(); ++it) {
    if (!it->empty())
      docs_list->AppendString(*it);
  }
  request_dict.Set(kAcceptedLegalDocumentKey, docs_list.release());

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetAcceptLegalDocumentsUrl(), post_body);
}

void WalletClient::MakeWalletRequest(const GURL& url,
                                     const std::string& post_body) {
  DCHECK(!HasRequestInProgress());

  request_.reset(net::URLFetcher::Create(
      0, url, net::URLFetcher::POST, this));
  request_->SetRequestContext(context_getter_);
  DVLOG(1) << "Making request to " << url << " with post_body=" << post_body;
  request_->SetUploadData(kJsonMimeType, post_body);
  request_->Start();

  delegate_->GetMetricLogger().LogWalletErrorMetric(
      delegate_->GetDialogType(),
      AutofillMetrics::WALLET_ERROR_BASELINE_ISSUED_REQUEST);
  delegate_->GetMetricLogger().LogWalletRequiredActionMetric(
      delegate_->GetDialogType(),
      AutofillMetrics::WALLET_REQUIRED_ACTION_BASELINE_ISSUED_REQUEST);
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
      HandleWalletError(WalletClient::BAD_REQUEST);
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

        std::string error_type;
        if (!response_dict->GetString(kErrorTypeKey, &error_type)) {
          HandleWalletError(WalletClient::UNKNOWN_ERROR);
          return;
        }

        HandleWalletError(StringToErrorType(error_type));
        return;
      }
      break;
    }

    // Anything else is an error.
    default:
      request_type_ = NO_PENDING_REQUEST;
      HandleNetworkError(response_code);
      return;
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
      delegate_->OnDidAcceptLegalDocuments();
      break;

    case AUTHENTICATE_INSTRUMENT: {
      std::string auth_result;
      if (response_dict->GetString(kAuthResultKey, &auth_result)) {
        std::string trimmed;
        TrimWhitespaceASCII(auth_result,
                            TRIM_ALL,
                            &trimmed);
        delegate_->OnDidAuthenticateInstrument(
            LowerCaseEqualsASCII(trimmed, "success"));
      } else {
        HandleMalformedResponse();
      }
      break;
    }

    case SEND_STATUS:
      delegate_->OnDidSendAutocheckoutStatus();
      break;

    case GET_FULL_WALLET: {
      scoped_ptr<FullWallet> full_wallet(
          FullWallet::CreateFullWallet(*response_dict));
      if (full_wallet) {
        full_wallet->set_one_time_pad(one_time_pad_);
        LogRequiredActions(full_wallet->required_actions());
        delegate_->OnDidGetFullWallet(full_wallet.Pass());
      } else {
        HandleMalformedResponse();
      }
      break;
    }

    case GET_WALLET_ITEMS: {
      scoped_ptr<WalletItems> wallet_items(
          WalletItems::CreateWalletItems(*response_dict));
      if (wallet_items) {
        LogRequiredActions(wallet_items->required_actions());
        delegate_->OnDidGetWalletItems(wallet_items.Pass());
      } else {
        HandleMalformedResponse();
      }
      break;
    }

    case SAVE_ADDRESS: {
      std::string shipping_address_id;
      std::vector<RequiredAction> required_actions;
      GetRequiredActionsForSaveToWallet(*response_dict, &required_actions);
      if (response_dict->GetString(kShippingAddressIdKey,
                                   &shipping_address_id) ||
          !required_actions.empty()) {
        LogRequiredActions(required_actions);
        delegate_->OnDidSaveAddress(shipping_address_id, required_actions);
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
        LogRequiredActions(required_actions);
        delegate_->OnDidSaveInstrument(instrument_id, required_actions);
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
        LogRequiredActions(required_actions);
        delegate_->OnDidSaveInstrumentAndAddress(instrument_id,
                                                 shipping_address_id,
                                                 required_actions);
      } else {
        HandleMalformedResponse();
      }
      break;
    }

    case UPDATE_ADDRESS: {
      std::string address_id;
      std::vector<RequiredAction> required_actions;
      GetRequiredActionsForSaveToWallet(*response_dict, &required_actions);
      if (response_dict->GetString(kShippingAddressIdKey, &address_id) ||
          !required_actions.empty()) {
        LogRequiredActions(required_actions);
        delegate_->OnDidUpdateAddress(address_id, required_actions);
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
        LogRequiredActions(required_actions);
        delegate_->OnDidUpdateInstrument(instrument_id, required_actions);
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
  delegate_->OnMalformedResponse();

  delegate_->GetMetricLogger().LogWalletErrorMetric(
      delegate_->GetDialogType(), AutofillMetrics::WALLET_MALFORMED_RESPONSE);
}

void WalletClient::HandleNetworkError(int response_code) {
  delegate_->OnNetworkError(response_code);
  delegate_->GetMetricLogger().LogWalletErrorMetric(
      delegate_->GetDialogType(), AutofillMetrics::WALLET_NETWORK_ERROR);
}

void WalletClient::HandleWalletError(WalletClient::ErrorType error_type) {
  delegate_->OnWalletError(error_type);
  delegate_->GetMetricLogger().LogWalletErrorMetric(
      delegate_->GetDialogType(), ErrorTypeToUmaMetric(error_type));
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

void WalletClient::OnDidMakeRequest() {
  delegate_->GetMetricLogger().LogWalletErrorMetric(
      delegate_->GetDialogType(),
      AutofillMetrics::WALLET_ERROR_BASELINE_ISSUED_REQUEST);
}

void WalletClient::OnNetworkError(int response_code) {
  HandleNetworkError(response_code);
}

void WalletClient::OnMalformedResponse() {
  delegate_->OnMalformedResponse();
  delegate_->GetMetricLogger().LogWalletErrorMetric(
      delegate_->GetDialogType(), AutofillMetrics::WALLET_MALFORMED_RESPONSE);
}

// Logs an UMA metric for each of the |required_actions|.
void WalletClient::LogRequiredActions(
    const std::vector<RequiredAction>& required_actions) const {
  for (size_t i = 0; i < required_actions.size(); ++i) {
    delegate_->GetMetricLogger().LogWalletRequiredActionMetric(
        delegate_->GetDialogType(),
        RequiredActionToUmaMetric(required_actions[i]));
  }
}

}  // namespace wallet
}  // namespace autofill
