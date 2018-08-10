// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/get_assertion_operation.h"

#include <set>
#include <string>

#import <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/string_number_conversions.h"
#include "device/fido/fido_constants.h"
#include "device/fido/mac/keychain.h"
#include "device/fido/mac/util.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace device {
namespace fido {
namespace mac {

using base::ScopedCFTypeRef;

GetAssertionOperation::GetAssertionOperation(CtapGetAssertionRequest request,
                                             std::string metadata_secret,
                                             std::string keychain_access_group,
                                             Callback callback)
    : OperationBase<CtapGetAssertionRequest, AuthenticatorGetAssertionResponse>(
          std::move(request),
          std::move(metadata_secret),
          std::move(keychain_access_group),
          std::move(callback)) {}
GetAssertionOperation::~GetAssertionOperation() = default;

const std::string& GetAssertionOperation::RpId() const {
  return request().rp_id();
}

void GetAssertionOperation::Run() {
  if (!Init()) {
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }

  // Collect the credential ids from allowList. If allowList is absent, we will
  // pick the first available credential for the RP.
  std::set<std::vector<uint8_t>> allowed_credential_ids;
  if (request().allow_list()) {
    for (const PublicKeyCredentialDescriptor& desc : *request().allow_list()) {
      if (desc.credential_type() != CredentialType::kPublicKey) {
        continue;
      }
      allowed_credential_ids.insert(desc.id());
    }
  }

  // Fetch credentials for RP from the request and current user profile.
  credential_ = FindCredentialInKeychain(
      keychain_access_group(), metadata_secret(), RpId(),
      allowed_credential_ids, authentication_context());
  if (!credential_) {
    // For now, don't show a Touch ID prompt if no credential exists.
    // TODO(martinkr): Prompt for the fingerprint anyway, once dispatch to this
    // authenticator is moved behind user interaction with the authenticator
    // selection UI.
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrNoCredentials, base::nullopt);
    return;
  }

  // Prompt the user for consent.
  // TODO(martinkr): Localize reason strings.
  PromptTouchId("sign in to " + RpId());
}

void GetAssertionOperation::PromptTouchIdDone(bool success) {
  DCHECK(credential_);

  if (!success) {
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOperationDenied, base::nullopt);
    return;
  }

  // Decrypt the user entity from the credential ID.
  base::Optional<CredentialMetadata::UserEntity> credential_user =
      CredentialMetadata::UnsealCredentialId(metadata_secret(), RpId(),
                                             credential_->credential_id);
  if (!credential_user) {
    // The keychain query already filtered for the RP ID encoded under this
    // operation's metadata secret, so the credential id really should have
    // been decryptable.
    DVLOG(1) << "UnsealCredentialId failed";
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrNoCredentials, base::nullopt);
    return;
  }

  base::ScopedCFTypeRef<SecKeyRef> public_key(
      Keychain::GetInstance().KeyCopyPublicKey(credential_->private_key));
  if (!public_key) {
    DLOG(ERROR) << "failed to get public key for credential id "
                << base::HexEncode(credential_->credential_id.data(),
                                   credential_->credential_id.size());
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }

  base::Optional<AuthenticatorData> authenticator_data = MakeAuthenticatorData(
      RpId(), credential_->credential_id, SecKeyRefToECPublicKey(public_key));
  if (!authenticator_data) {
    DLOG(ERROR) << "MakeAuthenticatorData failed";
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }
  base::Optional<std::vector<uint8_t>> signature =
      GenerateSignature(*authenticator_data, request().client_data_hash(),
                        credential_->private_key);
  if (!signature) {
    DLOG(ERROR) << "GenerateSignature failed";
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }
  auto response = AuthenticatorGetAssertionResponse(
      std::move(*authenticator_data), std::move(*signature));
  response.SetCredential(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey, std::move(credential_->credential_id)));
  response.SetUserEntity(credential_user->ToPublicKeyCredentialUserEntity());

  std::move(callback())
      .Run(CtapDeviceResponseCode::kSuccess, std::move(response));
}

}  // namespace mac
}  // namespace fido
}  // namespace device
