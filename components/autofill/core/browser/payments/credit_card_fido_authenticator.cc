// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_fido_authenticator.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "third_party/blink/public/mojom/webauthn/internal_authenticator.mojom.h"

namespace autofill {

namespace {
// Default timeout for user to respond to WebAuthn prompt.
constexpr int kWebAuthnTimeoutMs = 3 * 60 * 1000;  // 3 minutes
constexpr char kGooglePaymentsRpid[] = "google.com";

std::vector<uint8_t> Base64ToBytes(std::string base64) {
  std::string bytes;
  bool did_succeed = base::Base64Decode(base::StringPiece(base64), &bytes);
  if (did_succeed) {
    return std::vector<uint8_t>(bytes.begin(), bytes.end());
  }
  return std::vector<uint8_t>{};
}

base::Value BytesToBase64(const std::vector<uint8_t> bytes) {
  std::string base64;
  base::Base64Encode(std::string(bytes.begin(), bytes.end()), &base64);
  return base::Value(std::move(base64));
}
}  // namespace

CreditCardFIDOAuthenticator::CreditCardFIDOAuthenticator(AutofillDriver* driver,
                                                         AutofillClient* client)
    : autofill_driver_(driver),
      autofill_client_(client),
      payments_client_(client->GetPaymentsClient()) {}

CreditCardFIDOAuthenticator::~CreditCardFIDOAuthenticator() {}

void CreditCardFIDOAuthenticator::Authenticate(
    const CreditCard* card,
    base::WeakPtr<Requester> requester,
    base::TimeTicks form_parsed_timestamp,
    base::Value request_options) {
  card_ = card;
  requester_ = requester;
  form_parsed_timestamp_ = form_parsed_timestamp;

  if (card_ && IsValidRequestOptions(request_options.Clone())) {
    GetAssertion(ParseRequestOptions(std::move(request_options)));
  } else {
    requester_->OnFIDOAuthenticationComplete(/*did_succeed=*/false);
  }
}

void CreditCardFIDOAuthenticator::GetAssertion(
    PublicKeyCredentialRequestOptionsPtr request_options) {
  autofill_driver_->ConnectToAuthenticator(mojo::MakeRequest(&authenticator_));
  authenticator_->GetAssertion(
      std::move(request_options),
      base::BindOnce(&CreditCardFIDOAuthenticator::OnDidGetAssertion,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardFIDOAuthenticator::OnDidGetAssertion(
    AuthenticatorStatus status,
    GetAssertionAuthenticatorResponsePtr assertion_response) {
  if (status != AuthenticatorStatus::SUCCESS) {
    requester_->OnFIDOAuthenticationComplete(/*did_succeed=*/false);
    return;
  }

  base::Value response = ParseAssertionResponse(std::move(assertion_response));
  full_card_request_.reset(new payments::FullCardRequest(
      autofill_client_, autofill_client_->GetPaymentsClient(),
      autofill_client_->GetPersonalDataManager(), form_parsed_timestamp_));
  full_card_request_->GetFullCardViaFIDO(
      *card_, AutofillClient::UNMASK_FOR_AUTOFILL,
      weak_ptr_factory_.GetWeakPtr(), std::move(response));
}

void CreditCardFIDOAuthenticator::OnFullCardRequestSucceeded(
    const payments::FullCardRequest& full_card_request,
    const CreditCard& card,
    const base::string16& cvc) {
  requester_->OnFIDOAuthenticationComplete(/*did_succeed=*/true, &card);
}

void CreditCardFIDOAuthenticator::OnFullCardRequestFailed() {
  requester_->OnFIDOAuthenticationComplete(/*did_succeed=*/false);
}

PublicKeyCredentialRequestOptionsPtr
CreditCardFIDOAuthenticator::ParseRequestOptions(
    const base::Value& request_options) {
  auto options = PublicKeyCredentialRequestOptions::New();

  const auto* rpid = request_options.FindStringKey("relying_party_id");
  options->relying_party_id = rpid ? *rpid : std::string(kGooglePaymentsRpid);

  const auto* challenge = request_options.FindStringKey("challenge");
  DCHECK(challenge);
  options->challenge = Base64ToBytes(*challenge);

  const auto* timeout = request_options.FindKeyOfType(
      "timeout_millis", base::Value::Type::INTEGER);
  options->adjusted_timeout = base::TimeDelta::FromMilliseconds(
      timeout ? timeout->GetInt() : kWebAuthnTimeoutMs);

  options->user_verification = UserVerificationRequirement::kRequired;

  const auto* key_info_list =
      request_options.FindKeyOfType("key_info", base::Value::Type::LIST);
  DCHECK(key_info_list);
  for (const base::Value& key_info : key_info_list->GetList()) {
    options->allow_credentials.push_back(ParseCredentialDescriptor(key_info));
  }

  return options;
}

PublicKeyCredentialDescriptor
CreditCardFIDOAuthenticator::ParseCredentialDescriptor(
    const base::Value& key_info) {
  std::vector<uint8_t> credential_id;
  const auto* id = key_info.FindStringKey("credential_id");
  DCHECK(id);
  credential_id = Base64ToBytes(*id);

  base::flat_set<FidoTransportProtocol> authenticator_transports;
  const auto* transports = key_info.FindKeyOfType(
      "authenticator_transport_support", base::Value::Type::LIST);
  if (transports && !transports->GetList().empty()) {
    for (const base::Value& transport_type : transports->GetList()) {
      base::Optional<FidoTransportProtocol> protocol =
          device::ConvertToFidoTransportProtocol(
              base::ToLowerASCII(transport_type.GetString()));
      if (protocol.has_value())
        authenticator_transports.insert(*protocol);
    }
  }

  return PublicKeyCredentialDescriptor(CredentialType::kPublicKey,
                                       credential_id, authenticator_transports);
}

base::Value CreditCardFIDOAuthenticator::ParseAssertionResponse(
    GetAssertionAuthenticatorResponsePtr assertion_response) {
  base::Value response = base::Value(base::Value::Type::DICTIONARY);
  response.SetKey("credential_id", base::Value(assertion_response->info->id));
  response.SetKey("authenticator_data",
                  BytesToBase64(assertion_response->authenticator_data));
  response.SetKey("client_data",
                  BytesToBase64(assertion_response->info->client_data_json));
  response.SetKey("signature", BytesToBase64(assertion_response->signature));
  return response;
}

bool CreditCardFIDOAuthenticator::IsValidRequestOptions(
    const base::Value& request_options) {
  if (!request_options.is_dict() || request_options.DictEmpty() ||
      !request_options.FindStringKey("challenge") ||
      !request_options.FindKeyOfType("key_info", base::Value::Type::LIST)) {
    return false;
  }

  const auto* key_info_list =
      request_options.FindKeyOfType("key_info", base::Value::Type::LIST);

  if (key_info_list->GetList().empty())
    return false;

  for (const base::Value& key_info : key_info_list->GetList()) {
    if (!key_info.is_dict() || !key_info.FindStringKey("credential_id"))
      return false;
  }

  return true;
}

void CreditCardFIDOAuthenticator::IsUserVerifiable(
    base::OnceCallback<void(bool)> callback) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillCreditCardAuthentication)) {
    autofill_driver_->ConnectToAuthenticator(
        mojo::MakeRequest(&authenticator_));
    authenticator_->IsUserVerifyingPlatformAuthenticatorAvailable(
        std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

bool CreditCardFIDOAuthenticator::IsUserOptedIn() {
  return base::FeatureList::IsEnabled(
             features::kAutofillCreditCardAuthentication) &&
         ::autofill::prefs::IsCreditCardFIDOAuthEnabled(
             autofill_client_->GetPrefs());
}

void CreditCardFIDOAuthenticator::SyncUserOptIn(
    AutofillClient::UnmaskDetails& unmask_details) {
  bool is_user_opted_in = IsUserOptedIn();

  // If payments is offering to opt-in, then that means user is not opted in.
  if (unmask_details.offer_fido_opt_in) {
    is_user_opted_in = false;
  }

  // If payments is requesting a FIDO auth, then that means user is opted in.
  if (unmask_details.unmask_auth_method ==
      AutofillClient::UnmaskAuthMethod::FIDO) {
    is_user_opted_in = true;
  }

  // Update pref setting if needed.
  ::autofill::prefs::SetCreditCardFIDOAuthEnabled(autofill_client_->GetPrefs(),
                                                  is_user_opted_in);
}

}  // namespace autofill
