// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/make_credential_operation.h"

#include <string>

#import <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "device/fido/attestation_statement_formats.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/mac/credential_metadata.h"
#include "device/fido/mac/keychain.h"
#include "device/fido/mac/util.h"

namespace device {
namespace fido {
namespace mac {

using base::ScopedCFTypeRef;

MakeCredentialOperation::MakeCredentialOperation(
    CtapMakeCredentialRequest request,
    std::string metadata_secret,
    std::string keychain_access_group,
    Callback callback)
    : OperationBase<CtapMakeCredentialRequest,
                    AuthenticatorMakeCredentialResponse>(
          std::move(request),
          std::move(metadata_secret),
          std::move(keychain_access_group),
          std::move(callback)) {}
MakeCredentialOperation::~MakeCredentialOperation() = default;

const std::string& MakeCredentialOperation::RpId() const {
  return request().rp().rp_id();
}

void MakeCredentialOperation::Run() {
  if (!Init()) {
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }

  // Verify pubKeyCredParams contains ES-256, which is the only algorithm we
  // support.
  auto is_es256 =
      [](const PublicKeyCredentialParams::CredentialInfo& cred_info) {
        return cred_info.algorithm ==
               static_cast<int>(CoseAlgorithmIdentifier::kCoseEs256);
      };
  const auto& key_params =
      request().public_key_credential_params().public_key_credential_params();
  if (!std::any_of(key_params.begin(), key_params.end(), is_es256)) {
    DVLOG(1) << "No supported algorithm found.";
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrUnsupportedAlgorithms,
             base::nullopt);
    return;
  }

  // Prompt the user for consent.
  // TODO(martinkr): Localize reason strings.
  PromptTouchId("register with " + RpId());
}

void MakeCredentialOperation::PromptTouchIdDone(bool success) {
  if (!success) {
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOperationDenied, base::nullopt);
    return;
  }

  // Evaluate that excludeList does not contain any credentials stored by this
  // authenticator.
  if (request().exclude_list()) {
    for (auto& credential : *request().exclude_list()) {
      ScopedCFTypeRef<CFMutableDictionaryRef> query = DefaultKeychainQuery();
      CFDictionarySetValue(query, kSecAttrApplicationLabel,
                           [NSData dataWithBytes:credential.id().data()
                                          length:credential.id().size()]);
      OSStatus status = SecItemCopyMatching(query, nullptr);
      if (status == errSecSuccess) {
        // Excluded item found.
        DVLOG(1) << "credential from excludeList found";
        std::move(callback())
            .Run(CtapDeviceResponseCode::kCtap2ErrCredentialExcluded,
                 base::nullopt);
        return;
      }
      if (status != errSecItemNotFound) {
        // Unexpected keychain error.
        OSSTATUS_DLOG(ERROR, status)
            << "failed to check for excluded credential";
        std::move(callback())
            .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
        return;
      }
    }
  }

  // Delete the key pair for this RP + user handle if one already exists.
  base::Optional<std::string> encoded_rp_id_user_id =
      CredentialMetadata::EncodeRpIdAndUserId(metadata_secret(), RpId(),
                                              request().user().user_id());
  if (!encoded_rp_id_user_id) {
    // Internal error.
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }
  {
    ScopedCFTypeRef<CFMutableDictionaryRef> query = DefaultKeychainQuery();
    CFDictionarySetValue(query, kSecAttrApplicationTag,
                         base::SysUTF8ToNSString(*encoded_rp_id_user_id));
    OSStatus status = Keychain::GetInstance().ItemDelete(query);
    if (status != errSecSuccess && status != errSecItemNotFound) {
      // Internal keychain error.
      OSSTATUS_DLOG(ERROR, status) << "SecItemDelete failed";
      std::move(callback())
          .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
      return;
    }
  }

  // Generate the new key pair.
  base::Optional<std::vector<uint8_t>> credential_id =
      GenerateCredentialIdForRequest();
  if (!credential_id) {
    DLOG(ERROR) << "GenerateCredentialIdForRequest failed";
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }

  ScopedCFTypeRef<CFMutableDictionaryRef> params(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0, nullptr, nullptr));
  CFDictionarySetValue(params, kSecAttrKeyType,
                       kSecAttrKeyTypeECSECPrimeRandom);
  CFDictionarySetValue(params, kSecAttrKeySizeInBits, @256);
  CFDictionarySetValue(params, kSecAttrSynchronizable, @NO);
  CFDictionarySetValue(params, kSecAttrTokenID, kSecAttrTokenIDSecureEnclave);

  ScopedCFTypeRef<CFMutableDictionaryRef> private_key_params =
      DefaultKeychainQuery();
  CFDictionarySetValue(params, kSecPrivateKeyAttrs, private_key_params);
  CFDictionarySetValue(private_key_params, kSecAttrIsPermanent, @YES);
  CFDictionarySetValue(private_key_params, kSecAttrAccessControl,
                       access_control());
  CFDictionarySetValue(private_key_params, kSecUseAuthenticationContext,
                       authentication_context());
  CFDictionarySetValue(private_key_params, kSecAttrApplicationTag,
                       base::SysUTF8ToNSString(*encoded_rp_id_user_id));
  CFDictionarySetValue(private_key_params, kSecAttrApplicationLabel,
                       [NSData dataWithBytes:credential_id->data()
                                      length:credential_id->size()]);

  ScopedCFTypeRef<CFErrorRef> cferr;
  ScopedCFTypeRef<SecKeyRef> private_key(
      Keychain::GetInstance().KeyCreateRandomKey(params,
                                                 cferr.InitializeInto()));
  if (!private_key) {
    DLOG(ERROR) << "SecKeyCreateRandomKey failed: " << cferr;
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }
  ScopedCFTypeRef<SecKeyRef> public_key(
      Keychain::GetInstance().KeyCopyPublicKey(private_key));
  if (!public_key) {
    DLOG(ERROR) << "SecKeyCopyPublicKey failed";
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }

  // Create attestation object. There is no separate attestation key pair, so
  // we perform self-attestation.
  base::Optional<AuthenticatorData> authenticator_data = MakeAuthenticatorData(
      RpId(), *credential_id, SecKeyRefToECPublicKey(public_key));
  if (!authenticator_data) {
    DLOG(ERROR) << "MakeAuthenticatorData failed";
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }
  base::Optional<std::vector<uint8_t>> signature = GenerateSignature(
      *authenticator_data, request().client_data_hash(), private_key);
  if (!signature) {
    DLOG(ERROR) << "MakeSignature failed";
    std::move(callback())
        .Run(CtapDeviceResponseCode::kCtap2ErrOther, base::nullopt);
    return;
  }
  std::vector<std::vector<uint8_t>> no_certificates;
  AuthenticatorMakeCredentialResponse response(AttestationObject(
      std::move(*authenticator_data),
      std::make_unique<PackedAttestationStatement>(
          CoseAlgorithmIdentifier::kCoseEs256, std::move(*signature),
          std::move(no_certificates))));
  std::move(callback())
      .Run(CtapDeviceResponseCode::kSuccess, std::move(response));
}

base::Optional<std::vector<uint8_t>>
MakeCredentialOperation::GenerateCredentialIdForRequest() const {
  return CredentialMetadata::SealCredentialId(
      metadata_secret(), RpId(),
      CredentialMetadata::UserEntity::FromPublicKeyCredentialUserEntity(
          request().user()));
}

}  // namespace mac
}  // namespace fido
}  // namespace device
