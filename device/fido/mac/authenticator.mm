// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/authenticator.h"

#include <algorithm>

#import <LocalAuthentication/LocalAuthentication.h>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "device/base/features.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_constants.h"
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/get_assertion_operation.h"
#include "device/fido/mac/make_credential_operation.h"
#include "device/fido/mac/util.h"

namespace device {
namespace fido {
namespace mac {

// static
bool TouchIdAuthenticator::IsAvailable(const AuthenticatorConfig& config) {
  if (__builtin_available(macOS 10.12.2, *)) {
    return TouchIdContext::TouchIdAvailable(config);
  }
  return false;
}

// static
std::unique_ptr<TouchIdAuthenticator> TouchIdAuthenticator::CreateIfAvailable(
    AuthenticatorConfig config) {
  return IsAvailable(config) ? base::WrapUnique(new TouchIdAuthenticator(
                                   std::move(config.keychain_access_group),
                                   std::move(config.metadata_secret)))
                             : nullptr;
}

// static
std::unique_ptr<TouchIdAuthenticator> TouchIdAuthenticator::CreateForTesting(
    AuthenticatorConfig config) {
  return base::WrapUnique(
      new TouchIdAuthenticator(std::move(config.keychain_access_group),
                               std::move(config.metadata_secret)));
}

TouchIdAuthenticator::~TouchIdAuthenticator() = default;

bool TouchIdAuthenticator::HasCredentialForGetAssertionRequest(
    const CtapGetAssertionRequest& request) {
  if (__builtin_available(macOS 10.12.2, *)) {
    std::set<std::vector<uint8_t>> allowed_credential_ids;
    // Extract applicable credential IDs from the allowList, if the request has
    // one. If not, any credential matching the RP works.
    for (const auto& credential_descriptor : request.allow_list) {
      if (credential_descriptor.credential_type() ==
              CredentialType::kPublicKey &&
          (credential_descriptor.transports().empty() ||
           base::ContainsKey(credential_descriptor.transports(),
                             FidoTransportProtocol::kInternal))) {
        allowed_credential_ids.insert(credential_descriptor.id());
      }
    }
    if (allowed_credential_ids.empty()) {
      // TODO(martinkr): Implement resident keys for Touch ID.
      return false;
    }

    return FindCredentialInKeychain(keychain_access_group_, metadata_secret_,
                                    request.rp_id, allowed_credential_ids,
                                    nullptr /* LAContext */) != base::nullopt;
  }
  NOTREACHED();
  return false;
}

void TouchIdAuthenticator::InitializeAuthenticator(base::OnceClosure callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   std::move(callback));
}

void TouchIdAuthenticator::MakeCredential(CtapMakeCredentialRequest request,
                                          MakeCredentialCallback callback) {
  if (__builtin_available(macOS 10.12.2, *)) {
    DCHECK(!operation_);
    operation_ = std::make_unique<MakeCredentialOperation>(
        std::move(request), metadata_secret_, keychain_access_group_,
        std::move(callback));
    operation_->Run();
    return;
  }
  NOTREACHED();
}

void TouchIdAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                        GetAssertionCallback callback) {
  if (__builtin_available(macOS 10.12.2, *)) {
    DCHECK(!operation_);
    operation_ = std::make_unique<GetAssertionOperation>(
        std::move(request), metadata_secret_, keychain_access_group_,
        std::move(callback));
    operation_->Run();
    return;
  }
  NOTREACHED();
}

void TouchIdAuthenticator::Cancel() {
  // If there is an operation pending, delete it, which will clean up any
  // pending callbacks, e.g. if the operation is waiting for a response from
  // the Touch ID prompt. Note that we cannot cancel the actual prompt once it
  // has been shown.
  operation_.reset();
}

std::string TouchIdAuthenticator::GetId() const {
  return "TouchIdAuthenticator";
}

base::string16 TouchIdAuthenticator::GetDisplayName() const {
  return base::string16();
}

base::Optional<FidoTransportProtocol>
TouchIdAuthenticator::AuthenticatorTransport() const {
  return FidoTransportProtocol::kInternal;
}

namespace {

AuthenticatorSupportedOptions TouchIdAuthenticatorOptions() {
  AuthenticatorSupportedOptions options;
  options.is_platform_device = true;
  options.supports_resident_key = false;
  options.user_verification_availability = AuthenticatorSupportedOptions::
      UserVerificationAvailability::kSupportedAndConfigured;
  options.supports_user_presence = true;
  return options;
}

}  // namespace

const base::Optional<AuthenticatorSupportedOptions>&
TouchIdAuthenticator::Options() const {
  static const base::Optional<AuthenticatorSupportedOptions> options =
      TouchIdAuthenticatorOptions();
  return options;
}

bool TouchIdAuthenticator::IsInPairingMode() const {
  return false;
}

bool TouchIdAuthenticator::IsPaired() const {
  return false;
}

void TouchIdAuthenticator::GetTouch(base::OnceClosure callback) {
  NOTREACHED();
  std::move(callback).Run();
}

base::WeakPtr<FidoAuthenticator> TouchIdAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

TouchIdAuthenticator::TouchIdAuthenticator(std::string keychain_access_group,
                                           std::string metadata_secret)
    : keychain_access_group_(std::move(keychain_access_group)),
      metadata_secret_(std::move(metadata_secret)),
      weak_factory_(this) {}

}  // namespace mac
}  // namespace fido
}  // namespace device
