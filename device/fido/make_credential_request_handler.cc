// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/make_credential_request_handler.h"

#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/stl_util.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/make_credential_task.h"
#include "device/fido/pin.h"
#include "services/service_manager/public/cpp/connector.h"

namespace device {

using ClientPinAvailability =
    AuthenticatorSupportedOptions::ClientPinAvailability;
using MakeCredentialPINDisposition =
    FidoAuthenticator::MakeCredentialPINDisposition;

namespace {

// IsCandidateAuthenticatorPreTouch returns true if the given authenticator
// should even blink for a request.
bool IsCandidateAuthenticatorPreTouch(
    FidoAuthenticator* authenticator,
    const AuthenticatorSelectionCriteria& authenticator_selection_criteria) {
  const auto& opt_options = authenticator->Options();
  if (!opt_options) {
    // This authenticator doesn't know its capabilities yet, so we need
    // to assume it can handle the request. This is the case for Windows,
    // where we proxy the request to the native API.
    return true;
  }

  if ((authenticator_selection_criteria.authenticator_attachment() ==
           AuthenticatorAttachment::kPlatform &&
       !opt_options->is_platform_device) ||
      (authenticator_selection_criteria.authenticator_attachment() ==
           AuthenticatorAttachment::kCrossPlatform &&
       opt_options->is_platform_device)) {
    return false;
  }

  return true;
}

// IsCandidateAuthenticatorPostTouch returns a value other than |kSuccess| if
// the given authenticator cannot handle a request.
FidoReturnCode IsCandidateAuthenticatorPostTouch(
    const CtapMakeCredentialRequest& request,
    FidoAuthenticator* authenticator,
    const AuthenticatorSelectionCriteria& authenticator_selection_criteria,
    const FidoRequestHandlerBase::Observer* observer) {
  const auto& opt_options = authenticator->Options();
  if (!opt_options) {
    // This authenticator doesn't know its capabilities yet, so we need
    // to assume it can handle the request. This is the case for Windows,
    // where we proxy the request to the native API.
    return FidoReturnCode::kSuccess;
  }

  if (authenticator_selection_criteria.require_resident_key() &&
      !opt_options->supports_resident_key) {
    return FidoReturnCode::kAuthenticatorMissingResidentKeys;
  }

  if (authenticator->WillNeedPINToMakeCredential(request, observer) ==
      MakeCredentialPINDisposition::kUnsatisfiable) {
    return FidoReturnCode::kAuthenticatorMissingUserVerification;
  }

  return FidoReturnCode::kSuccess;
}

base::flat_set<FidoTransportProtocol> GetTransportsAllowedByRP(
    const AuthenticatorSelectionCriteria& authenticator_selection_criteria) {
  const auto attachment_type =
      authenticator_selection_criteria.authenticator_attachment();
  switch (attachment_type) {
    case AuthenticatorAttachment::kPlatform:
      return {FidoTransportProtocol::kInternal};
    case AuthenticatorAttachment::kCrossPlatform:
      // Cloud-assisted BLE is not yet supported for MakeCredential requests.
      return {FidoTransportProtocol::kUsbHumanInterfaceDevice,
              FidoTransportProtocol::kBluetoothLowEnergy,
              FidoTransportProtocol::kNearFieldCommunication};
    case AuthenticatorAttachment::kAny:
      // Cloud-assisted BLE is not yet supported for MakeCredential requests.
      return {FidoTransportProtocol::kInternal,
              FidoTransportProtocol::kNearFieldCommunication,
              FidoTransportProtocol::kUsbHumanInterfaceDevice,
              FidoTransportProtocol::kBluetoothLowEnergy};
  }

  NOTREACHED();
  return base::flat_set<FidoTransportProtocol>();
}

}  // namespace

MakeCredentialRequestHandler::MakeCredentialRequestHandler(
    service_manager::Connector* connector,
    const base::flat_set<FidoTransportProtocol>& supported_transports,
    CtapMakeCredentialRequest request,
    AuthenticatorSelectionCriteria authenticator_selection_criteria,
    CompletionCallback completion_callback)
    : FidoRequestHandler(
          connector,
          base::STLSetIntersection<base::flat_set<FidoTransportProtocol>>(
              supported_transports,
              GetTransportsAllowedByRP(authenticator_selection_criteria)),
          std::move(completion_callback)),
      request_(std::move(request)),
      authenticator_selection_criteria_(
          std::move(authenticator_selection_criteria)),
      weak_factory_(this) {
  transport_availability_info().request_type =
      FidoRequestHandlerBase::RequestType::kMakeCredential;

  // Set the rk, uv and attachment fields, which were only initialized to
  // default values up to here.  TODO(martinkr): Initialize these fields earlier
  // (in AuthenticatorImpl) and get rid of the separate
  // AuthenticatorSelectionCriteriaParameter.
  if (authenticator_selection_criteria_.require_resident_key()) {
    request_.SetResidentKeyRequired(true);
    request_.SetUserVerification(UserVerificationRequirement::kRequired);
  } else {
    request_.SetResidentKeyRequired(false);
    request_.SetUserVerification(
        authenticator_selection_criteria_.user_verification_requirement());
  }
  request_.SetAuthenticatorAttachment(
      authenticator_selection_criteria_.authenticator_attachment());

  Start();
}

MakeCredentialRequestHandler::~MakeCredentialRequestHandler() = default;

void MakeCredentialRequestHandler::DispatchRequest(
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch ||
      !IsCandidateAuthenticatorPreTouch(authenticator,
                                        authenticator_selection_criteria_)) {
    return;
  }

  if (IsCandidateAuthenticatorPostTouch(
          request_, authenticator, authenticator_selection_criteria_,
          observer()) != FidoReturnCode::kSuccess) {
    if (!base::FeatureList::IsEnabled(device::kWebAuthPINSupport)) {
      // Don't flash authenticator without PIN support. This maintains previous
      // behaviour and avoids adding UI unprotected by a feature flag without
      // increasing the number of feature flags.
      FIDO_LOG(DEBUG) << "Dropping " << authenticator->GetDisplayName()
                      << " because it does not meet selection criteria and PIN "
                         "support is not enabled";
      return;
    }

    // This authenticator does not meet requirements, but make it flash anyway
    // so the user understands that it's functional. A descriptive error message
    // will be shown if the user selects it.
    authenticator->GetTouch(base::BindOnce(
        &MakeCredentialRequestHandler::HandleInapplicableAuthenticator,
        weak_factory_.GetWeakPtr(), authenticator));
    return;
  }

  if (base::FeatureList::IsEnabled(device::kWebAuthPINSupport)) {
    switch (authenticator->WillNeedPINToMakeCredential(request_, observer())) {
      case MakeCredentialPINDisposition::kUsePIN:
      case MakeCredentialPINDisposition::kSetPIN:
        // A PIN will be needed. Just request a touch to let the user select
        // this authenticator if they wish.
        authenticator->GetTouch(
            base::BindOnce(&MakeCredentialRequestHandler::HandleTouch,
                           weak_factory_.GetWeakPtr(), authenticator));
        return;

      case MakeCredentialPINDisposition::kNoPIN:
        break;

      case MakeCredentialPINDisposition::kUnsatisfiable:
        // |IsCandidateAuthenticatorPostTouch| should have handled this case.
        NOTREACHED();
        return;
    }
  }

  CtapMakeCredentialRequest request(request_);
  if (authenticator->Options()) {
    // If the authenticator has UV configured then UV will be required in
    // order to create a credential (as specified by CTAP 2.0), even if
    // user-verification is "discouraged". However, if the request is U2F-only
    // then that doesn't apply and UV must be set to discouraged so that the
    // request can be translated to U2F.
    if (authenticator->Options()->user_verification_availability ==
            AuthenticatorSupportedOptions::UserVerificationAvailability::
                kSupportedAndConfigured &&
        !request_.is_u2f_only()) {
      request.SetUserVerification(UserVerificationRequirement::kRequired);
    } else {
      request.SetUserVerification(UserVerificationRequirement::kDiscouraged);
    }
  }
  authenticator->MakeCredential(
      std::move(request),
      base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator));
}

void MakeCredentialRequestHandler::AuthenticatorRemoved(
    FidoDiscoveryBase* discovery,
    FidoAuthenticator* authenticator) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  FidoRequestHandlerBase::AuthenticatorRemoved(discovery, authenticator);

  if (authenticator == authenticator_) {
    authenticator_ = nullptr;
    if (state_ == State::kWaitingForPIN || state_ == State::kWaitingForNewPIN ||
        state_ == State::kWaitingForSecondTouch) {
      state_ = State::kFinished;
      std::move(completion_callback_)
          .Run(FidoReturnCode::kAuthenticatorRemovedDuringPINEntry,
               base::nullopt, base::nullopt);
    }
  }
}

void MakeCredentialRequestHandler::HandleResponse(
    FidoAuthenticator* authenticator,
    CtapDeviceResponseCode status,
    base::Optional<AuthenticatorMakeCredentialResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (state_ != State::kWaitingForTouch &&
      state_ != State::kWaitingForSecondTouch) {
    return;
  }

  // Requests that require a PIN should follow the |GetTouch| path initially.
  DCHECK(state_ == State::kWaitingForSecondTouch ||
         !base::FeatureList::IsEnabled(device::kWebAuthPINSupport) ||
         authenticator->WillNeedPINToMakeCredential(request_, observer()) ==
             MakeCredentialPINDisposition::kNoPIN);

  const base::Optional<FidoReturnCode> maybe_result =
      ConvertDeviceResponseCodeToFidoReturnCode(status);
  if (!maybe_result) {
    FIDO_LOG(ERROR) << "Ignoring status " << static_cast<int>(status)
                    << " from " << authenticator->GetDisplayName();
    return;
  }

  state_ = State::kFinished;
  CancelActiveAuthenticators(authenticator->GetId());

  if (status != CtapDeviceResponseCode::kSuccess) {
    FIDO_LOG(ERROR) << "Failing make credential request due to status "
                    << static_cast<int>(status) << " from "
                    << authenticator->GetDisplayName();
    OnAuthenticatorResponse(authenticator, *maybe_result, base::nullopt);
    return;
  }

  const auto rp_id_hash =
      fido_parsing_utils::CreateSHA256Hash(request_.rp().rp_id());

  if (!response || response->GetRpIdHash() != rp_id_hash) {
    FIDO_LOG(ERROR) << "Failing assertion request due to bad response from "
                    << authenticator->GetDisplayName();
    OnAuthenticatorResponse(authenticator,
                            FidoReturnCode::kAuthenticatorResponseInvalid,
                            base::nullopt);
    return;
  }

  OnAuthenticatorResponse(authenticator, FidoReturnCode::kSuccess,
                          std::move(response));
}

void MakeCredentialRequestHandler::HandleTouch(
    FidoAuthenticator* authenticator) {
  if (state_ != State::kWaitingForTouch) {
    return;
  }

  DCHECK(base::FeatureList::IsEnabled(device::kWebAuthPINSupport));

  switch (authenticator->WillNeedPINToMakeCredential(request_, observer())) {
    case MakeCredentialPINDisposition::kUsePIN:
      // Will need to get PIN to handle this request.
      DCHECK(observer());
      state_ = State::kGettingRetries;
      CancelActiveAuthenticators(authenticator->GetId());
      authenticator_ = authenticator;
      authenticator_->GetRetries(
          base::BindOnce(&MakeCredentialRequestHandler::OnRetriesResponse,
                         weak_factory_.GetWeakPtr()));
      return;

    case MakeCredentialPINDisposition::kSetPIN:
      // Will need to set a PIN to handle this request.
      DCHECK(observer());
      state_ = State::kWaitingForNewPIN;
      CancelActiveAuthenticators(authenticator->GetId());
      authenticator_ = authenticator;
      observer()->CollectPIN(
          base::nullopt,
          base::BindOnce(&MakeCredentialRequestHandler::OnHavePIN,
                         weak_factory_.GetWeakPtr()));
      return;

    case MakeCredentialPINDisposition::kNoPIN:
    case MakeCredentialPINDisposition::kUnsatisfiable:
      // No PIN needed for this request.
      NOTREACHED();
      break;
  }
}

void MakeCredentialRequestHandler::HandleInapplicableAuthenticator(
    FidoAuthenticator* authenticator) {
  // User touched an authenticator that cannot handle this request.
  state_ = State::kFinished;
  CancelActiveAuthenticators(authenticator->GetId());
  const FidoReturnCode capability_error = IsCandidateAuthenticatorPostTouch(
      request_, authenticator, authenticator_selection_criteria_, observer());
  DCHECK_NE(capability_error, FidoReturnCode::kSuccess);
  std::move(completion_callback_)
      .Run(capability_error, base::nullopt, base::nullopt);
}

void MakeCredentialRequestHandler::OnHavePIN(std::string pin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK(state_ == State::kWaitingForPIN || state_ == State::kWaitingForNewPIN);
  DCHECK(pin::IsValid(pin));

  if (authenticator_ == nullptr) {
    // Authenticator was detached. The request will already have been canceled
    // but this callback may have been waiting in a queue.
    DCHECK(!completion_callback_);
    return;
  }

  if (state_ == State::kWaitingForPIN) {
    state_ = State::kGetEphemeralKey;
  } else {
    DCHECK_EQ(state_, State::kWaitingForNewPIN);
    state_ = State::kGetEphemeralKeyForNewPIN;
  }

  authenticator_->GetEphemeralKey(
      base::BindOnce(&MakeCredentialRequestHandler::OnHaveEphemeralKey,
                     weak_factory_.GetWeakPtr(), std::move(pin)));
}

void MakeCredentialRequestHandler::OnRetriesResponse(
    CtapDeviceResponseCode status,
    base::Optional<pin::RetriesResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(state_, State::kGettingRetries);

  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    FidoReturnCode ret = FidoReturnCode::kAuthenticatorResponseInvalid;
    if (status == CtapDeviceResponseCode::kCtap2ErrPinBlocked) {
      ret = FidoReturnCode::kHardPINBlock;
    }
    std::move(completion_callback_).Run(ret, base::nullopt, base::nullopt);
    return;
  }

  state_ = State::kWaitingForPIN;
  observer()->CollectPIN(
      response->retries,
      base::BindOnce(&MakeCredentialRequestHandler::OnHavePIN,
                     weak_factory_.GetWeakPtr()));
}

void MakeCredentialRequestHandler::OnHaveEphemeralKey(
    std::string pin,
    CtapDeviceResponseCode status,
    base::Optional<pin::KeyAgreementResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK(state_ == State::kGetEphemeralKey ||
         state_ == State::kGetEphemeralKeyForNewPIN);

  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    std::move(completion_callback_)
        .Run(FidoReturnCode::kAuthenticatorResponseInvalid, base::nullopt,
             base::nullopt);
    return;
  }

  if (state_ == State::kGetEphemeralKey) {
    state_ = State::kRequestWithPIN;
    authenticator_->GetPINToken(
        std::move(pin), *response,
        base::BindOnce(&MakeCredentialRequestHandler::OnHavePINToken,
                       weak_factory_.GetWeakPtr()));
  } else {
    DCHECK_EQ(state_, State::kGetEphemeralKeyForNewPIN);
    state_ = State::kSettingPIN;
    authenticator_->SetPIN(
        pin, *response,
        base::BindOnce(&MakeCredentialRequestHandler::OnHaveSetPIN,
                       weak_factory_.GetWeakPtr(), pin, *response));
  }
}

void MakeCredentialRequestHandler::OnHaveSetPIN(
    std::string pin,
    pin::KeyAgreementResponse key_agreement,
    CtapDeviceResponseCode status,
    base::Optional<pin::EmptyResponse> response) {
  DCHECK_EQ(state_, State::kSettingPIN);

  // Having just set the PIN, we need to immediately turn around and use it to
  // get a PIN token.
  state_ = State::kRequestWithPIN;
  authenticator_->GetPINToken(
      std::move(pin), key_agreement,
      base::BindOnce(&MakeCredentialRequestHandler::OnHavePINToken,
                     weak_factory_.GetWeakPtr()));
}

void MakeCredentialRequestHandler::OnHavePINToken(
    CtapDeviceResponseCode status,
    base::Optional<pin::TokenResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);
  DCHECK_EQ(state_, State::kRequestWithPIN);

  if (status == CtapDeviceResponseCode::kCtap2ErrPinInvalid) {
    state_ = State::kGettingRetries;
    authenticator_->GetRetries(
        base::BindOnce(&MakeCredentialRequestHandler::OnRetriesResponse,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  if (status != CtapDeviceResponseCode::kSuccess) {
    state_ = State::kFinished;
    FidoReturnCode ret;
    switch (status) {
      case CtapDeviceResponseCode::kCtap2ErrPinAuthBlocked:
        ret = FidoReturnCode::kSoftPINBlock;
        break;
      case CtapDeviceResponseCode::kCtap2ErrPinBlocked:
        ret = FidoReturnCode::kHardPINBlock;
        break;
      default:
        ret = FidoReturnCode::kAuthenticatorResponseInvalid;
        break;
    }
    std::move(completion_callback_).Run(ret, base::nullopt, base::nullopt);
    return;
  }

  observer()->FinishCollectPIN();
  state_ = State::kWaitingForSecondTouch;
  CtapMakeCredentialRequest request(request_);
  request.SetPinAuth(response->PinAuth(request.client_data_hash()));
  request.SetPinProtocol(pin::kProtocolVersion);
  // If doing a PIN operation then we don't ask the authenticator to also do
  // internal UV.
  request.SetUserVerification(UserVerificationRequirement::kDiscouraged);

  authenticator_->MakeCredential(
      std::move(request),
      base::BindOnce(&MakeCredentialRequestHandler::HandleResponse,
                     weak_factory_.GetWeakPtr(), authenticator_));
}

void MakeCredentialRequestHandler::SetPlatformAuthenticatorOrMarkUnavailable(
    base::Optional<PlatformAuthenticatorInfo> platform_authenticator_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (platform_authenticator_info) {
    // TODO(crbug.com/873710): In the case of a request with
    // AuthenticatorAttachment::kAny and when there is no embedder-provided
    // transport selection UI, disable the platform authenticator to avoid the
    // Touch ID fingerprint prompt competing with external devices.
    const bool has_transport_selection_ui =
        observer() && observer()->EmbedderControlsAuthenticatorDispatch(
                          *platform_authenticator_info->authenticator);
    if (authenticator_selection_criteria_.authenticator_attachment() ==
            AuthenticatorAttachment::kAny &&
        !has_transport_selection_ui) {
      platform_authenticator_info = base::nullopt;
    }
  }

  FidoRequestHandlerBase::SetPlatformAuthenticatorOrMarkUnavailable(
      std::move(platform_authenticator_info));
}

}  // namespace device
