// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/make_credential_operation.h"

#include <string>

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "device/fido/fido_attestation_statement.h"
#include "device/fido/fido_constants.h"
#include "device/fido/mac/util.h"

namespace device {
namespace fido {
namespace mac {

using base::ScopedCFTypeRef;

MakeCredentialOperation::MakeCredentialOperation(
    CtapMakeCredentialRequest request,
    std::string profile_id,
    std::string keychain_access_group,
    MakeCredentialOperation::Callback callback)
    : profile_id_(std::move(profile_id)),
      keychain_access_group_(std::move(keychain_access_group)),
      request_(std::move(request)),
      callback_(std::move(callback)),
      context_([[LAContext alloc] init]),
      access_(nil) {}

MakeCredentialOperation::MakeCredentialOperation(MakeCredentialOperation&&) =
    default;

MakeCredentialOperation& MakeCredentialOperation::operator=(
    MakeCredentialOperation&&) = default;

MakeCredentialOperation::~MakeCredentialOperation() = default;

void MakeCredentialOperation::Run() {
  // Verify pubKeyCredParams contains ES-256, which is the only algorithm we
  // support.
  auto is_es256 =
      [](const PublicKeyCredentialParams::CredentialInfo& cred_info) {
        return cred_info.algorithm ==
               static_cast<int>(CoseAlgorithmIdentifier::kCoseEs256);
      };
  const auto& key_params =
      request_.public_key_credential_params().public_key_credential_params();
  if (!std::any_of(key_params.begin(), key_params.end(), is_es256)) {
    DVLOG(1) << "No supported algorithm found.";
    std::move(callback_).Run(
        CtapDeviceResponseCode::kCtap2ErrUnsupportedAlgorithms, base::nullopt);
    return;
  }

  // Prompt the user for consent. The SecAccessControlRef used for the Touch
  // ID prompt is stashed away in |access_| so it can be reused for signing
  // the attestation object later, without triggering a second Touch ID
  // prompt.
  ScopedCFTypeRef<CFErrorRef> cferr;
  access_.reset(SecAccessControlCreateWithFlags(
      kCFAllocatorDefault, kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
      kSecAccessControlPrivateKeyUsage | kSecAccessControlTouchIDAny,
      cferr.InitializeInto()));
  if (!access_) {
    DLOG(ERROR) << "SecAccessControlCreateWithFlags failed: " << cferr;
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }
  // TODO(martinkr): Localize reason strings.
  std::string reason = "register with " + request_.rp().rp_id();
  // TODO(martinkr): The callback block implicitly captures |this|, which is a
  // use-after-destroy issue. The TouchIdContext class extracted in a follow-up
  // CL holds a WeakPtr instead.
  [context_ evaluateAccessControl:access_
                        operation:LAAccessControlOperationUseKeySign
                  localizedReason:base::SysUTF8ToNSString(reason)
                            reply:^(BOOL success, NSError* error) {
                              PromptTouchIdDone(success, error);
                            }];
}

void MakeCredentialOperation::PromptTouchIdDone(bool success, NSError* err) {
  if (!success) {
    // err is autoreleased.
    CHECK(err != nil);
    DVLOG(1) << "Touch ID prompt failed: " << base::mac::NSToCFCast(err);
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOperationDenied,
                             base::nullopt);
    return;
  }

  // Evaluate that excludeList does not contain any credentials stored by this
  // authenticator.
  if (request_.exclude_list()) {
    for (auto& credential : *request_.exclude_list()) {
      // TODO(martinkr): Extract helper for query dictionary creation.
      ScopedCFTypeRef<CFMutableDictionaryRef> query(
          CFDictionaryCreateMutable(kCFAllocatorDefault, 0, nullptr, nullptr));
      CFDictionarySetValue(query, kSecClass, kSecClassKey);
      CFDictionarySetValue(query, kSecAttrApplicationLabel,
                           [NSData dataWithBytes:credential.id().data()
                                          length:credential.id().size()]);
      CFDictionarySetValue(query, kSecAttrLabel,
                           base::SysUTF8ToNSString(request_.rp().rp_id()));
      CFDictionarySetValue(query, kSecAttrApplicationTag,
                           base::SysUTF8ToNSString(profile_id_));
      CFDictionarySetValue(query, kSecAttrAccessGroup,
                           base::SysUTF8ToNSString(keychain_access_group_));
      OSStatus status = SecItemCopyMatching(query, nullptr);
      if (status == errSecSuccess) {
        // Excluded item found.
        DVLOG(1) << "credential from excludeList found";
        std::move(callback_).Run(
            CtapDeviceResponseCode::kCtap2ErrCredentialExcluded, base::nullopt);
        return;
      }
      if (status != errSecItemNotFound) {
        // Unexpected keychain error.
        OSSTATUS_DLOG(ERROR, status)
            << "failed to check for excluded credential";
        std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                                 base::nullopt);
        return;
      }
    }
  }

  // Delete the key pair for this RP + user handle if one already exists.
  const std::vector<uint8_t> keychain_item_id =
      KeychainItemIdentifier(request_.rp().rp_id(), request_.user().user_id());
  {
    ScopedCFTypeRef<CFMutableDictionaryRef> query(
        CFDictionaryCreateMutable(kCFAllocatorDefault, 0, nullptr, nullptr));
    CFDictionarySetValue(query, kSecClass, kSecClassKey);
    CFDictionarySetValue(query, kSecAttrApplicationLabel,
                         [NSData dataWithBytes:keychain_item_id.data()
                                        length:keychain_item_id.size()]);
    CFDictionarySetValue(query, kSecAttrLabel,
                         base::SysUTF8ToNSString(request_.rp().rp_id()));
    CFDictionarySetValue(query, kSecAttrApplicationTag,
                         base::SysUTF8ToNSString(profile_id_));
    CFDictionarySetValue(query, kSecAttrAccessGroup,
                         base::SysUTF8ToNSString(keychain_access_group_));
    OSStatus status = SecItemDelete(query);
    if (status != errSecSuccess && status != errSecItemNotFound) {
      // Internal keychain error.
      OSSTATUS_DLOG(ERROR, status) << "SecItemDelete failed";
      std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                               base::nullopt);
      return;
    }
  }

  // Generate the new key pair.
  ScopedCFTypeRef<CFMutableDictionaryRef> params(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0, nullptr, nullptr));
  CFDictionarySetValue(params, kSecAttrKeyType,
                       kSecAttrKeyTypeECSECPrimeRandom);
  CFDictionarySetValue(params, kSecAttrKeySizeInBits, @256);
  CFDictionarySetValue(params, kSecAttrSynchronizable, @NO);
  CFDictionarySetValue(params, kSecAttrTokenID, kSecAttrTokenIDSecureEnclave);
  ScopedCFTypeRef<CFMutableDictionaryRef> private_key_params(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0, nullptr, nullptr));
  CFDictionarySetValue(params, kSecPrivateKeyAttrs, private_key_params);
  CFDictionarySetValue(private_key_params, kSecAttrIsPermanent, @YES);
  // These are the only fields we have for metadata storage.
  // ApplicationLabel must be unique (globally or per access group?); all
  // allow lookup.
  CFDictionarySetValue(private_key_params, kSecAttrApplicationLabel,
                       [NSData dataWithBytes:keychain_item_id.data()
                                      length:keychain_item_id.size()]);
  CFDictionarySetValue(private_key_params, kSecAttrLabel,
                       base::SysUTF8ToNSString(request_.rp().rp_id()));
  CFDictionarySetValue(private_key_params, kSecAttrApplicationTag,
                       base::SysUTF8ToNSString(profile_id_));
  CFDictionarySetValue(private_key_params, kSecAttrAccessControl, access_);
  CFDictionarySetValue(private_key_params, kSecAttrAccessGroup,
                       base::SysUTF8ToNSString(keychain_access_group_));
  CFDictionarySetValue(private_key_params, kSecUseAuthenticationContext,
                       context_);
  ScopedCFTypeRef<CFErrorRef> cferr;
  ScopedCFTypeRef<SecKeyRef> private_key(
      SecKeyCreateRandomKey(params, cferr.InitializeInto()));
  if (!private_key) {
    DLOG(ERROR) << "SecKeyCreateRandomKey failed: " << cferr;
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }
  ScopedCFTypeRef<SecKeyRef> public_key(SecKeyCopyPublicKey(private_key));
  if (!public_key) {
    DLOG(ERROR) << "SecKeyCopyPublicKey failed";
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }

  // Create attestation object. There is no separate attestation key pair, so
  // we perform self-attestation.
  base::Optional<AuthenticatorData> authenticator_data = MakeAuthenticatorData(
      request_.client_data_hash(), keychain_item_id, public_key);
  if (!authenticator_data) {
    DLOG(ERROR) << "MakeAuthenticatorData failed";
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }
  base::Optional<std::vector<uint8_t>> signature = GenerateSignature(
      *authenticator_data, request_.client_data_hash(), private_key);
  if (!signature) {
    DLOG(ERROR) << "MakeSignature failed";
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }
  std::vector<std::vector<uint8_t>> no_certificates;
  AuthenticatorMakeCredentialResponse response(AttestationObject(
      std::move(*authenticator_data),
      // TODO(martinkr): Add a PackedAttestationStatement for self-attestation.
      std::make_unique<FidoAttestationStatement>(std::move(*signature),
                                                 std::move(no_certificates))));
  std::move(callback_).Run(CtapDeviceResponseCode::kSuccess,
                           std::move(response));
}

}  // namespace mac
}  // namespace fido
}  // namespace device
