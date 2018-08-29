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
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "device/fido/fido_constants.h"
#include "device/fido/mac/keychain.h"
#include "device/fido/mac/util.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/strings/grit/fido_strings.h"
#include "ui/base/l10n/l10n_util.h"

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

  // Display the macOS Touch ID prompt.
  PromptTouchId(l10n_util::GetStringFUTF16(IDS_WEBAUTHN_TOUCH_ID_PROMPT_REASON,
                                           base::UTF8ToUTF16(RpId())));
}

void GetAssertionOperation::PromptTouchIdDone(bool success) {
  if (!success) {
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOperationDenied, base::nullopt);
    return;
  }

  // Collect the credential ids from allowList. If allowList is absent, we will
  // pick the first available credential for the RP.
  std::set<std::vector<uint8_t>> allowed_credential_ids;
  if (request().allow_list()) {
    for (const PublicKeyCredentialDescriptor& desc : *request().allow_list()) {
      if (desc.credential_type() != CredentialType::kPublicKey)
        continue;

      if (!desc.transports().empty() &&
          !base::ContainsKey(desc.transports(),
                             FidoTransportProtocol::kInternal))
        continue;

      allowed_credential_ids.insert(desc.id());
    }
  }

  // Fetch credentials for RP from the request and current user profile.
  base::Optional<Credential> credential = FindCredentialInKeychain(
      keychain_access_group(), metadata_secret(), RpId(),
      allowed_credential_ids, authentication_context());
  if (!credential) {
    // For now, don't show a Touch ID prompt if no credential exists.
    // TODO(martinkr): Prompt for the fingerprint anyway, once dispatch to this
    // authenticator is moved behind user interaction with the authenticator
    // selection UI.
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrNoCredentials, base::nullopt);
    return;
  }

  // Decrypt the user entity from the credential ID.
  base::Optional<CredentialMetadata::UserEntity> credential_user =
      CredentialMetadata::UnsealCredentialId(metadata_secret(), RpId(),
                                             credential->credential_id);
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
      Keychain::GetInstance().KeyCopyPublicKey(credential->private_key));
  if (!public_key) {
    DLOG(ERROR) << "failed to get public key for credential id "
                << base::HexEncode(credential->credential_id.data(),
                                   credential->credential_id.size());
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }

  base::Optional<AuthenticatorData> authenticator_data = MakeAuthenticatorData(
      RpId(), credential->credential_id, SecKeyRefToECPublicKey(public_key));
  if (!authenticator_data) {
    DLOG(ERROR) << "MakeAuthenticatorData failed";
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }
  base::Optional<std::vector<uint8_t>> signature =
      GenerateSignature(*authenticator_data, request().client_data_hash(),
                        credential->private_key);
  if (!signature) {
    DLOG(ERROR) << "GenerateSignature failed";
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }
  auto response = AuthenticatorGetAssertionResponse(
      std::move(*authenticator_data), std::move(*signature));
  response.SetCredential(PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey, std::move(credential->credential_id)));
  response.SetUserEntity(credential_user->ToPublicKeyCredentialUserEntity());

  std::move(callback())
      .Run(CtapDeviceResponseCode::kSuccess, std::move(response));
}

}  // namespace mac
}  // namespace fido
}  // namespace device
