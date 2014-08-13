// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/wallet/wallet_client.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/browser/wallet/form_field_error.h"
#include "components/autofill/content/browser/wallet/instrument.h"
#include "components/autofill/content/browser/wallet/wallet_address.h"
#include "components/autofill/content/browser/wallet/wallet_client_delegate.h"
#include "components/autofill/content/browser/wallet/wallet_items.h"
#include "components/autofill/content/browser/wallet/wallet_service_url.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "crypto/random.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace autofill {
namespace wallet {

namespace {

const char kFormEncodedMimeType[] = "application/x-www-form-urlencoded";
const char kJsonMimeType[] = "application/json";
const char kEscrowNewInstrumentFormat[] =
    "request_content_type=application/json&request=%s&cvn=%s&card_number=%s";
const char kEscrowCardVerificationNumberFormat[] =
    "request_content_type=application/json&request=%s&cvn=%s";
const char kGetFullWalletRequestFormat[] =
    "request_content_type=application/json&request=%s&otp=%s:%s";
const size_t kOneTimePadLength = 6;

// The maximum number of bits in the one time pad that the server is willing to
// accept.
const size_t kMaxBits = 56;

// The minimum number of bits in the one time pad that the server is willing to
// accept.
const size_t kMinBits = 40;

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
  base::TrimWhitespaceASCII(error_type, base::TRIM_ALL, &trimmed);
  if (base::LowerCaseEqualsASCII(trimmed, "buyer_account_error"))
    return WalletClient::BUYER_ACCOUNT_ERROR;
  if (base::LowerCaseEqualsASCII(trimmed, "unsupported_merchant"))
    return WalletClient::UNSUPPORTED_MERCHANT;
  if (base::LowerCaseEqualsASCII(trimmed, "internal_error"))
    return WalletClient::INTERNAL_ERROR;
  if (base::LowerCaseEqualsASCII(trimmed, "invalid_params"))
    return WalletClient::INVALID_PARAMS;
  if (base::LowerCaseEqualsASCII(trimmed, "service_unavailable"))
    return WalletClient::SERVICE_UNAVAILABLE;
  if (base::LowerCaseEqualsASCII(trimmed, "unsupported_api_version"))
    return WalletClient::UNSUPPORTED_API_VERSION;
  if (base::LowerCaseEqualsASCII(trimmed, "unsupported_user_agent"))
    return WalletClient::UNSUPPORTED_USER_AGENT_OR_API_KEY;
  if (base::LowerCaseEqualsASCII(trimmed, "spending_limit_exceeded"))
    return WalletClient::SPENDING_LIMIT_EXCEEDED;

  DVLOG(1) << "Unknown wallet error string: \"" << error_type << '"';
  return WalletClient::UNKNOWN_ERROR;
}

// Get the more specific WalletClient::ErrorType when the error is
// |BUYER_ACCOUNT_ERROR|.
WalletClient::ErrorType BuyerErrorStringToErrorType(
    const std::string& message_type_for_buyer) {
  std::string trimmed;
  base::TrimWhitespaceASCII(message_type_for_buyer, base::TRIM_ALL, &trimmed);
  if (base::LowerCaseEqualsASCII(trimmed, "bla_country_not_supported"))
    return WalletClient::BUYER_LEGAL_ADDRESS_NOT_SUPPORTED;
  if (base::LowerCaseEqualsASCII(trimmed, "buyer_kyc_error"))
    return WalletClient::UNVERIFIED_KNOW_YOUR_CUSTOMER_STATUS;

  return WalletClient::BUYER_ACCOUNT_ERROR;
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

void GetFormFieldErrors(const base::DictionaryValue& dict,
                        std::vector<FormFieldError>* form_errors) {
  DCHECK(form_errors->empty());
  const base::ListValue* form_errors_list;
  if (!dict.GetList("form_field_error", &form_errors_list))
    return;

  for (size_t i = 0; i < form_errors_list->GetSize(); ++i) {
    const base::DictionaryValue* dictionary;
    if (form_errors_list->GetDictionary(i, &dictionary))
      form_errors->push_back(FormFieldError::CreateFormFieldError(*dictionary));
  }
}

// Converts the |error_type| to the corresponding value from the stable UMA
// metric enumeration.
AutofillMetrics::WalletErrorMetric ErrorTypeToUmaMetric(
    WalletClient::ErrorType error_type) {
  switch (error_type) {
    case WalletClient::BAD_REQUEST:
      return AutofillMetrics::WALLET_BAD_REQUEST;
    case WalletClient::BUYER_LEGAL_ADDRESS_NOT_SUPPORTED:
      return AutofillMetrics::WALLET_BUYER_LEGAL_ADDRESS_NOT_SUPPORTED;
    case WalletClient::BUYER_ACCOUNT_ERROR:
      return AutofillMetrics::WALLET_BUYER_ACCOUNT_ERROR;
    case WalletClient::INTERNAL_ERROR:
      return AutofillMetrics::WALLET_INTERNAL_ERROR;
    case WalletClient::INVALID_PARAMS:
      return AutofillMetrics::WALLET_INVALID_PARAMS;
    case WalletClient::UNVERIFIED_KNOW_YOUR_CUSTOMER_STATUS:
      return AutofillMetrics::WALLET_UNVERIFIED_KNOW_YOUR_CUSTOMER_STATUS;
    case WalletClient::SERVICE_UNAVAILABLE:
      return AutofillMetrics::WALLET_SERVICE_UNAVAILABLE;
    case WalletClient::UNSUPPORTED_API_VERSION:
      return AutofillMetrics::WALLET_UNSUPPORTED_API_VERSION;
    case WalletClient::UNSUPPORTED_MERCHANT:
      return AutofillMetrics::WALLET_UNSUPPORTED_MERCHANT;
    case WalletClient::SPENDING_LIMIT_EXCEEDED:
      return AutofillMetrics::WALLET_SPENDING_LIMIT_EXCEEDED;
    case WalletClient::MALFORMED_RESPONSE:
      return AutofillMetrics::WALLET_MALFORMED_RESPONSE;
    case WalletClient::NETWORK_ERROR:
      return AutofillMetrics::WALLET_NETWORK_ERROR;
    case WalletClient::UNKNOWN_ERROR:
      return AutofillMetrics::WALLET_UNKNOWN_ERROR;
    case WalletClient::UNSUPPORTED_USER_AGENT_OR_API_KEY:
      return AutofillMetrics::WALLET_UNSUPPORTED_USER_AGENT_OR_API_KEY;
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
      return AutofillMetrics::UNKNOWN_REQUIRED_ACTION;
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
  return AutofillMetrics::UNKNOWN_REQUIRED_ACTION;
}

// Keys for JSON communication with the Online Wallet server.
const char kAcceptedLegalDocumentKey[] = "accepted_legal_document";
const char kApiKeyKey[] = "api_key";
const char kAuthResultKey[] = "auth_result";
const char kErrorTypeKey[] = "wallet_error.error_type";
const char kFeatureKey[] = "feature";
const char kGoogleTransactionIdKey[] = "google_transaction_id";
const char kInstrumentIdKey[] = "instrument_id";
const char kInstrumentKey[] = "instrument";
const char kInstrumentExpMonthKey[] = "instrument.credit_card.exp_month";
const char kInstrumentExpYearKey[] = "instrument.credit_card.exp_year";
const char kInstrumentType[] = "instrument.type";
const char kInstrumentPhoneNumberKey[] = "instrument_phone_number";
const char kMerchantDomainKey[] = "merchant_domain";
const char kMessageTypeForBuyerKey[] = "wallet_error.message_type_for_buyer";
const char kNewWalletUser[] = "new_wallet_user";
const char kPhoneNumberRequired[] = "phone_number_required";
const char kRiskCapabilitiesKey[] = "supported_risk_challenge";
const char kRiskParamsKey[] = "risk_params";
const char kSelectedAddressIdKey[] = "selected_address_id";
const char kSelectedInstrumentIdKey[] = "selected_instrument_id";
const char kShippingAddressIdKey[] = "shipping_address_id";
const char kShippingAddressKey[] = "shipping_address";
const char kShippingAddressRequired[] = "shipping_address_required";
const char kTransactionAmountKey[] = "estimated_total_price";
const char kTransactionCurrencyKey[] = "currency_code";
const char kUpgradedBillingAddressKey[] = "upgraded_billing_address";
const char kUpgradedInstrumentIdKey[] = "upgraded_instrument_id";
const char kUseMinimalAddresses[] = "use_minimal_addresses";

}  // namespace

WalletClient::FullWalletRequest::FullWalletRequest(
    const std::string& instrument_id,
    const std::string& address_id,
    const std::string& google_transaction_id,
    const std::vector<RiskCapability> risk_capabilities,
    bool new_wallet_user)
    : instrument_id(instrument_id),
      address_id(address_id),
      google_transaction_id(google_transaction_id),
      risk_capabilities(risk_capabilities),
      new_wallet_user(new_wallet_user) {}

WalletClient::FullWalletRequest::~FullWalletRequest() {}

WalletClient::WalletClient(net::URLRequestContextGetter* context_getter,
                           WalletClientDelegate* delegate,
                           const GURL& source_url)
    : context_getter_(context_getter),
      delegate_(delegate),
      user_index_(0U),
      source_url_(source_url),
      request_type_(NO_REQUEST),
      one_time_pad_(kOneTimePadLength),
      weak_ptr_factory_(this) {
  DCHECK(context_getter_.get());
  DCHECK(delegate_);
}

WalletClient::~WalletClient() {}

void WalletClient::AcceptLegalDocuments(
    const std::vector<WalletItems::LegalDocument*>& documents,
    const std::string& google_transaction_id) {
  if (documents.empty())
    return;

  std::vector<std::string> document_ids;
  for (size_t i = 0; i < documents.size(); ++i) {
    document_ids.push_back(documents[i]->id());
  }
  DoAcceptLegalDocuments(document_ids, google_transaction_id);
}

void WalletClient::AuthenticateInstrument(
    const std::string& instrument_id,
    const std::string& card_verification_number) {
  base::DictionaryValue request_dict;
  request_dict.SetString(kApiKeyKey, google_apis::GetAPIKey());
  request_dict.SetString(kRiskParamsKey, delegate_->GetRiskData());
  request_dict.SetString(kInstrumentIdKey, instrument_id);

  std::string json_payload;
  base::JSONWriter::Write(&request_dict, &json_payload);

  std::string escaped_card_verification_number = net::EscapeUrlEncodedData(
      card_verification_number, true);

  std::string post_body = base::StringPrintf(
      kEscrowCardVerificationNumberFormat,
      net::EscapeUrlEncodedData(json_payload, true).c_str(),
      escaped_card_verification_number.c_str());

  MakeWalletRequest(GetAuthenticateInstrumentUrl(user_index_),
                    post_body,
                    kFormEncodedMimeType,
                    AUTHENTICATE_INSTRUMENT);
}

void WalletClient::GetFullWallet(const FullWalletRequest& full_wallet_request) {
  base::DictionaryValue request_dict;
  request_dict.SetString(kApiKeyKey, google_apis::GetAPIKey());
  request_dict.SetString(kRiskParamsKey, delegate_->GetRiskData());
  request_dict.SetBoolean(kUseMinimalAddresses, false);
  request_dict.SetBoolean(kPhoneNumberRequired, true);
  request_dict.SetBoolean(kNewWalletUser, full_wallet_request.new_wallet_user);

  request_dict.SetString(kSelectedInstrumentIdKey,
                         full_wallet_request.instrument_id);
  request_dict.SetString(kSelectedAddressIdKey, full_wallet_request.address_id);
  request_dict.SetString(
      kMerchantDomainKey,
      source_url_.GetWithEmptyPath().spec());
  request_dict.SetString(kGoogleTransactionIdKey,
                         full_wallet_request.google_transaction_id);
  request_dict.SetString(kFeatureKey, "REQUEST_AUTOCOMPLETE");

  scoped_ptr<base::ListValue> risk_capabilities_list(new base::ListValue());
  for (std::vector<RiskCapability>::const_iterator it =
           full_wallet_request.risk_capabilities.begin();
       it != full_wallet_request.risk_capabilities.end();
       ++it) {
    risk_capabilities_list->AppendString(RiskCapabilityToString(*it));
  }
  request_dict.Set(kRiskCapabilitiesKey, risk_capabilities_list.release());

  std::string json_payload;
  base::JSONWriter::Write(&request_dict, &json_payload);

  crypto::RandBytes(&(one_time_pad_[0]), one_time_pad_.size());

  size_t num_bits = one_time_pad_.size() * 8;
  DCHECK_LE(num_bits, kMaxBits);
  DCHECK_GE(num_bits, kMinBits);

  std::string post_body = base::StringPrintf(
      kGetFullWalletRequestFormat,
      net::EscapeUrlEncodedData(json_payload, true).c_str(),
      base::HexEncode(&num_bits, 1).c_str(),
      base::HexEncode(&(one_time_pad_[0]), one_time_pad_.size()).c_str());

  MakeWalletRequest(GetGetFullWalletUrl(user_index_),
                    post_body,
                    kFormEncodedMimeType,
                    GET_FULL_WALLET);
}

void WalletClient::SaveToWallet(
    scoped_ptr<Instrument> instrument,
    scoped_ptr<Address> address,
    const WalletItems::MaskedInstrument* reference_instrument,
    const Address* reference_address) {
  DCHECK(instrument || address);

  base::DictionaryValue request_dict;
  request_dict.SetString(kApiKeyKey, google_apis::GetAPIKey());
  request_dict.SetString(kRiskParamsKey, delegate_->GetRiskData());
  request_dict.SetString(kMerchantDomainKey,
                         source_url_.GetWithEmptyPath().spec());
  request_dict.SetBoolean(kUseMinimalAddresses, false);
  request_dict.SetBoolean(kPhoneNumberRequired, true);

  std::string primary_account_number;
  std::string card_verification_number;
  if (instrument) {
    primary_account_number = net::EscapeUrlEncodedData(
        base::UTF16ToUTF8(instrument->primary_account_number()), true);
    card_verification_number = net::EscapeUrlEncodedData(
        base::UTF16ToUTF8(instrument->card_verification_number()), true);

    if (!reference_instrument) {
      request_dict.Set(kInstrumentKey, instrument->ToDictionary().release());
      request_dict.SetString(kInstrumentPhoneNumberKey,
                             instrument->address()->phone_number());
    } else {
      DCHECK(!reference_instrument->object_id().empty());

      int new_month = instrument->expiration_month();
      int new_year = instrument->expiration_year();
      bool expiration_date_changed =
          new_month != reference_instrument->expiration_month() ||
          new_year != reference_instrument->expiration_year();

      DCHECK(instrument->address() || expiration_date_changed);

      request_dict.SetString(kUpgradedInstrumentIdKey,
                             reference_instrument->object_id());

      if (instrument->address()) {
        request_dict.SetString(kInstrumentPhoneNumberKey,
                               instrument->address()->phone_number());
        request_dict.Set(
            kUpgradedBillingAddressKey,
            instrument->address()->ToDictionaryWithoutID().release());
      }

      if (expiration_date_changed) {
        // Updating expiration date requires a CVC.
        DCHECK(!instrument->card_verification_number().empty());
        request_dict.SetInteger(kInstrumentExpMonthKey,
                                instrument->expiration_month());
        request_dict.SetInteger(kInstrumentExpYearKey,
                                instrument->expiration_year());
      }

      if (request_dict.HasKey(kInstrumentKey))
        request_dict.SetString(kInstrumentType, "CREDIT_CARD");
    }
  }
  if (address) {
    if (reference_address) {
      address->set_object_id(reference_address->object_id());
      DCHECK(!address->object_id().empty());
    }
    request_dict.Set(kShippingAddressKey,
                     address->ToDictionaryWithID().release());
  }

  std::string json_payload;
  base::JSONWriter::Write(&request_dict, &json_payload);

  if (!card_verification_number.empty()) {
    std::string post_body;
    if (!primary_account_number.empty()) {
      post_body = base::StringPrintf(
          kEscrowNewInstrumentFormat,
          net::EscapeUrlEncodedData(json_payload, true).c_str(),
          card_verification_number.c_str(),
          primary_account_number.c_str());
    } else {
      post_body = base::StringPrintf(
          kEscrowCardVerificationNumberFormat,
          net::EscapeUrlEncodedData(json_payload, true).c_str(),
          card_verification_number.c_str());
    }
    MakeWalletRequest(GetSaveToWalletUrl(user_index_),
                      post_body,
                      kFormEncodedMimeType,
                      SAVE_TO_WALLET);
  } else {
    MakeWalletRequest(GetSaveToWalletNoEscrowUrl(user_index_),
                      json_payload,
                      kJsonMimeType,
                      SAVE_TO_WALLET);
  }
}

void WalletClient::GetWalletItems(const base::string16& amount,
                                  const base::string16& currency) {
  base::DictionaryValue request_dict;
  request_dict.SetString(kApiKeyKey, google_apis::GetAPIKey());
  request_dict.SetString(kMerchantDomainKey,
                         source_url_.GetWithEmptyPath().spec());
  request_dict.SetBoolean(kShippingAddressRequired,
                          delegate_->IsShippingAddressRequired());
  request_dict.SetBoolean(kUseMinimalAddresses, false);
  request_dict.SetBoolean(kPhoneNumberRequired, true);

  if (!amount.empty())
    request_dict.SetString(kTransactionAmountKey, amount);
  if (!currency.empty())
    request_dict.SetString(kTransactionCurrencyKey, currency);

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetGetWalletItemsUrl(user_index_),
                    post_body,
                    kJsonMimeType,
                    GET_WALLET_ITEMS);
}

bool WalletClient::HasRequestInProgress() const {
  return request_;
}

void WalletClient::CancelRequest() {
  request_.reset();
  request_type_ = NO_REQUEST;
}

void WalletClient::SetUserIndex(size_t user_index) {
  CancelRequest();
  user_index_ = user_index;
}

void WalletClient::DoAcceptLegalDocuments(
    const std::vector<std::string>& document_ids,
    const std::string& google_transaction_id) {
  base::DictionaryValue request_dict;
  request_dict.SetString(kApiKeyKey, google_apis::GetAPIKey());
  request_dict.SetString(kGoogleTransactionIdKey, google_transaction_id);
  request_dict.SetString(kMerchantDomainKey,
                         source_url_.GetWithEmptyPath().spec());
  scoped_ptr<base::ListValue> docs_list(new base::ListValue());
  for (std::vector<std::string>::const_iterator it = document_ids.begin();
       it != document_ids.end(); ++it) {
    if (!it->empty())
      docs_list->AppendString(*it);
  }
  request_dict.Set(kAcceptedLegalDocumentKey, docs_list.release());

  std::string post_body;
  base::JSONWriter::Write(&request_dict, &post_body);

  MakeWalletRequest(GetAcceptLegalDocumentsUrl(user_index_),
                    post_body,
                    kJsonMimeType,
                    ACCEPT_LEGAL_DOCUMENTS);
}

void WalletClient::MakeWalletRequest(const GURL& url,
                                     const std::string& post_body,
                                     const std::string& mime_type,
                                     RequestType request_type) {
  DCHECK_EQ(request_type_, NO_REQUEST);
  request_type_ = request_type;

  request_.reset(net::URLFetcher::Create(
      0, url, net::URLFetcher::POST, this));
  request_->SetRequestContext(context_getter_.get());
  DVLOG(1) << "Making request to " << url << " with post_body=" << post_body;
  request_->SetUploadData(mime_type, post_body);
  request_->AddExtraRequestHeader("Authorization: GoogleLogin auth=" +
                                  delegate_->GetWalletCookieValue());
  DVLOG(1) << "Setting authorization header value to "
           << delegate_->GetWalletCookieValue();
  request_started_timestamp_ = base::Time::Now();
  request_->Start();

  delegate_->GetMetricLogger().LogWalletErrorMetric(
      AutofillMetrics::WALLET_ERROR_BASELINE_ISSUED_REQUEST);
  delegate_->GetMetricLogger().LogWalletRequiredActionMetric(
      AutofillMetrics::WALLET_REQUIRED_ACTION_BASELINE_ISSUED_REQUEST);
}

// TODO(ahutter): Add manual retry logic if it's necessary.
void WalletClient::OnURLFetchComplete(
    const net::URLFetcher* source) {
  delegate_->GetMetricLogger().LogWalletApiCallDuration(
      RequestTypeToUmaMetric(request_type_),
      base::Time::Now() - request_started_timestamp_);

  DCHECK_EQ(source, request_.get());
  DVLOG(1) << "Got response from " << source->GetOriginalURL();

  // |request_|, which is aliased to |source|, might continue to be used in this
  // |method, but should be freed once control leaves the method.
  scoped_ptr<net::URLFetcher> scoped_request(request_.Pass());

  std::string data;
  source->GetResponseAsString(&data);
  DVLOG(1) << "Response body: " << data;

  scoped_ptr<base::DictionaryValue> response_dict;

  int response_code = source->GetResponseCode();
  delegate_->GetMetricLogger().LogWalletResponseCode(response_code);

  switch (response_code) {
    // HTTP_BAD_REQUEST means the arguments are invalid. No point retrying.
    case net::HTTP_BAD_REQUEST: {
      request_type_ = NO_REQUEST;
      HandleWalletError(BAD_REQUEST);
      return;
    }
    // HTTP_OK holds a valid response and HTTP_INTERNAL_SERVER_ERROR holds an
    // error code and message for the user.
    case net::HTTP_OK:
    case net::HTTP_INTERNAL_SERVER_ERROR: {
      scoped_ptr<base::Value> message_value(base::JSONReader::Read(data));
      if (message_value.get() &&
          message_value->IsType(base::Value::TYPE_DICTIONARY)) {
        response_dict.reset(
            static_cast<base::DictionaryValue*>(message_value.release()));
      }
      if (response_code == net::HTTP_INTERNAL_SERVER_ERROR) {
        request_type_ = NO_REQUEST;

        std::string error_type_string;
        if (!response_dict->GetString(kErrorTypeKey, &error_type_string)) {
          HandleWalletError(UNKNOWN_ERROR);
          return;
        }
        WalletClient::ErrorType error_type =
            StringToErrorType(error_type_string);
        if (error_type == BUYER_ACCOUNT_ERROR) {
          // If the error_type is |BUYER_ACCOUNT_ERROR|, then
          // message_type_for_buyer field contains more specific information
          // about the error.
          std::string message_type_for_buyer_string;
          if (response_dict->GetString(kMessageTypeForBuyerKey,
                                       &message_type_for_buyer_string)) {
            error_type = BuyerErrorStringToErrorType(
                message_type_for_buyer_string);
          }
        }

        HandleWalletError(error_type);
        return;
      }
      break;
    }

    // Anything else is an error.
    default:
      request_type_ = NO_REQUEST;
      HandleWalletError(NETWORK_ERROR);
      return;
  }

  RequestType type = request_type_;
  request_type_ = NO_REQUEST;

  if (type != ACCEPT_LEGAL_DOCUMENTS && !response_dict) {
    HandleMalformedResponse(type, scoped_request.get());
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
        base::TrimWhitespaceASCII(auth_result, base::TRIM_ALL, &trimmed);
        delegate_->OnDidAuthenticateInstrument(
            base::LowerCaseEqualsASCII(trimmed, "success"));
      } else {
        HandleMalformedResponse(type, scoped_request.get());
      }
      break;
    }

    case GET_FULL_WALLET: {
      scoped_ptr<FullWallet> full_wallet(
          FullWallet::CreateFullWallet(*response_dict));
      if (full_wallet) {
        full_wallet->set_one_time_pad(one_time_pad_);
        LogRequiredActions(full_wallet->required_actions());
        delegate_->OnDidGetFullWallet(full_wallet.Pass());
      } else {
        HandleMalformedResponse(type, scoped_request.get());
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
        HandleMalformedResponse(type, scoped_request.get());
      }
      break;
    }

    case SAVE_TO_WALLET: {
      std::string instrument_id;
      response_dict->GetString(kInstrumentIdKey, &instrument_id);
      std::string shipping_address_id;
      response_dict->GetString(kShippingAddressIdKey,
                               &shipping_address_id);
      std::vector<RequiredAction> required_actions;
      GetRequiredActionsForSaveToWallet(*response_dict, &required_actions);
      std::vector<FormFieldError> form_errors;
      GetFormFieldErrors(*response_dict, &form_errors);
      if (instrument_id.empty() && shipping_address_id.empty() &&
          required_actions.empty()) {
        HandleMalformedResponse(type, scoped_request.get());
      } else {
        LogRequiredActions(required_actions);
        delegate_->OnDidSaveToWallet(instrument_id,
                                     shipping_address_id,
                                     required_actions,
                                     form_errors);
      }
      break;
    }

    case NO_REQUEST:
      NOTREACHED();
  }
}

void WalletClient::HandleMalformedResponse(RequestType request_type,
                                           net::URLFetcher* request) {
  // Called to inform exponential backoff logic of the error.
  request->ReceivedContentWasMalformed();
  // Record failed API call in metrics.
  delegate_->GetMetricLogger().LogWalletMalformedResponseMetric(
    RequestTypeToUmaMetric(request_type));
  HandleWalletError(MALFORMED_RESPONSE);
}

void WalletClient::HandleWalletError(WalletClient::ErrorType error_type) {
  std::string error_message;
  switch (error_type) {
    case WalletClient::BAD_REQUEST:
      error_message = "WALLET_BAD_REQUEST";
      break;
    case WalletClient::BUYER_LEGAL_ADDRESS_NOT_SUPPORTED:
      error_message = "WALLET_BUYER_LEGAL_ADDRESS_NOT_SUPPORTED";
      break;
    case WalletClient::BUYER_ACCOUNT_ERROR:
      error_message = "WALLET_BUYER_ACCOUNT_ERROR";
      break;
    case WalletClient::INTERNAL_ERROR:
      error_message = "WALLET_INTERNAL_ERROR";
      break;
    case WalletClient::INVALID_PARAMS:
      error_message = "WALLET_INVALID_PARAMS";
      break;
    case WalletClient::UNVERIFIED_KNOW_YOUR_CUSTOMER_STATUS:
      error_message = "WALLET_UNVERIFIED_KNOW_YOUR_CUSTOMER_STATUS";
      break;
    case WalletClient::SPENDING_LIMIT_EXCEEDED:
      error_message = "SPENDING_LIMIT_EXCEEDED";
      break;
    case WalletClient::SERVICE_UNAVAILABLE:
      error_message = "WALLET_SERVICE_UNAVAILABLE";
      break;
    case WalletClient::UNSUPPORTED_API_VERSION:
      error_message = "WALLET_UNSUPPORTED_API_VERSION";
      break;
    case WalletClient::UNSUPPORTED_MERCHANT:
      error_message = "WALLET_UNSUPPORTED_MERCHANT";
      break;
    case WalletClient::MALFORMED_RESPONSE:
      error_message = "WALLET_MALFORMED_RESPONSE";
      break;
    case WalletClient::NETWORK_ERROR:
      error_message = "WALLET_NETWORK_ERROR";
      break;
    case WalletClient::UNKNOWN_ERROR:
      error_message = "WALLET_UNKNOWN_ERROR";
      break;
    case WalletClient::UNSUPPORTED_USER_AGENT_OR_API_KEY:
      error_message = "WALLET_UNSUPPORTED_USER_AGENT_OR_API_KEY";
      break;
  }

  DVLOG(1) << "Wallet encountered a " << error_message;

  delegate_->OnWalletError(error_type);
  delegate_->GetMetricLogger().LogWalletErrorMetric(
      ErrorTypeToUmaMetric(error_type));
}

// Logs an UMA metric for each of the |required_actions|.
void WalletClient::LogRequiredActions(
    const std::vector<RequiredAction>& required_actions) const {
  for (size_t i = 0; i < required_actions.size(); ++i) {
    delegate_->GetMetricLogger().LogWalletRequiredActionMetric(
        RequiredActionToUmaMetric(required_actions[i]));
  }
}

AutofillMetrics::WalletApiCallMetric WalletClient::RequestTypeToUmaMetric(
    RequestType request_type) const {
  switch (request_type) {
    case ACCEPT_LEGAL_DOCUMENTS:
      return AutofillMetrics::ACCEPT_LEGAL_DOCUMENTS;
    case AUTHENTICATE_INSTRUMENT:
      return AutofillMetrics::AUTHENTICATE_INSTRUMENT;
    case GET_FULL_WALLET:
      return AutofillMetrics::GET_FULL_WALLET;
    case GET_WALLET_ITEMS:
      return AutofillMetrics::GET_WALLET_ITEMS;
    case SAVE_TO_WALLET:
      return AutofillMetrics::SAVE_TO_WALLET;
    case NO_REQUEST:
      NOTREACHED();
      return AutofillMetrics::UNKNOWN_API_CALL;
  }

  NOTREACHED();
  return AutofillMetrics::UNKNOWN_API_CALL;
}

}  // namespace wallet
}  // namespace autofill
