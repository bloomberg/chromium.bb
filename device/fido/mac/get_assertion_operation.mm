// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/get_assertion_operation.h"

#include <set>
#include <string>

#import <Foundation/Foundation.h>

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "device/fido/ec_public_key.h"
#include "device/fido/fido_attestation_statement.h"
#include "device/fido/fido_constants.h"
#include "device/fido/mac/util.h"

namespace device {
namespace fido {
namespace mac {

using base::ScopedCFTypeRef;

GetAssertionOperation::GetAssertionOperation(
    CtapGetAssertionRequest request,
    std::string profile_id,
    std::string keychain_access_group,
    GetAssertionOperation::Callback callback)
    : profile_id_(std::move(profile_id)),
      keychain_access_group_(std::move(keychain_access_group)),
      request_(std::move(request)),
      callback_(std::move(callback)),
      context_([[LAContext alloc] init]),
      access_(nil) {}

GetAssertionOperation::GetAssertionOperation(GetAssertionOperation&&) = default;

GetAssertionOperation& GetAssertionOperation::operator=(
    GetAssertionOperation&&) = default;

GetAssertionOperation::~GetAssertionOperation() = default;

void GetAssertionOperation::Run() {
  // Prompt the user for consent. The SecAccessControlRef used for the Touch ID
  // prompt is stashed away in |access_| so it can be reused for signing the
  // attestation object later, without triggering a second Touch ID prompt.
  //
  // N.B. that the CTAP spec explicitly says consent has to be acquired *first*
  // before trying to match credentials, while webauthn does it later.
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
  std::string reason = "sign in to " + request_.rp_id();
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

void GetAssertionOperation::PromptTouchIdDone(bool success, NSError* err) {
  if (!success) {
    // err is autoreleased.
    CHECK(err != nil);
    DVLOG(1) << "Touch ID prompt failed: " << base::mac::NSToCFCast(err);
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOperationDenied,
                             base::nullopt);
    return;
  }

  // Collect the credential ids from allowList. If allowList is absent, we will
  // just pick the first available credential for the RP below.
  std::set<std::vector<uint8_t>> allowed_credential_ids;
  if (request_.allow_list()) {
    for (const PublicKeyCredentialDescriptor& desc : *request_.allow_list()) {
      if (desc.credential_type() != "public-key") {
        continue;
      }
      allowed_credential_ids.insert(desc.id());
    }
  }

  // Fetch credentials for RP from the request and current user profile.
  ScopedCFTypeRef<CFArrayRef> keychain_items;
  base::ScopedCFTypeRef<CFMutableDictionaryRef> query(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0, nullptr, nullptr));
  CFDictionarySetValue(query, kSecClass, kSecClassKey);
  CFDictionarySetValue(query, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
  CFDictionarySetValue(query, kSecAttrLabel,
                       base::SysUTF8ToNSString(request_.rp_id()));
  CFDictionarySetValue(query, kSecAttrApplicationTag,
                       base::SysUTF8ToNSString(profile_id_));
  CFDictionarySetValue(query, kSecUseAuthenticationContext, context_);
  CFDictionarySetValue(query, kSecReturnRef, @YES);
  CFDictionarySetValue(query, kSecReturnAttributes, @YES);
  CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);
  OSStatus status = SecItemCopyMatching(
      query, reinterpret_cast<CFTypeRef*>(keychain_items.InitializeInto()));
  if (status == errSecItemNotFound) {
    DVLOG(1) << "no credentials found for RP";
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrNoCredentials,
                             base::nullopt);
    return;
  }
  if (status != errSecSuccess) {
    OSSTATUS_DLOG(ERROR, status) << "SecItemCopyMatching failed";
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }
  SecKeyRef private_key = nil;  // Owned by |keychain_items|.
  std::vector<uint8_t> credential_id;
  for (CFIndex i = 0; i < CFArrayGetCount(keychain_items); ++i) {
    CFDictionaryRef attributes = base::mac::CFCast<CFDictionaryRef>(
        CFArrayGetValueAtIndex(keychain_items, i));
    CFDataRef application_label = base::mac::GetValueFromDictionary<CFDataRef>(
        attributes, kSecAttrApplicationLabel);
    SecKeyRef key =
        base::mac::GetValueFromDictionary<SecKeyRef>(attributes, kSecValueRef);
    if (!application_label || !key) {
      // Corrupted keychain?
      DLOG(ERROR) << "could not find application label or key ref: "
                  << attributes;
      continue;
    }
    std::vector<uint8_t> cid(CFDataGetBytePtr(application_label),
                             CFDataGetBytePtr(application_label) +
                                 CFDataGetLength(application_label));
    if (allowed_credential_ids.empty() ||
        allowed_credential_ids.find(cid) != allowed_credential_ids.end()) {
      private_key = key;
      credential_id = std::move(cid);
      break;
    }
  }
  if (!private_key) {
    DVLOG(1) << "no allowed credential found";
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrNoCredentials,
                             base::nullopt);
    return;
  }
  base::ScopedCFTypeRef<SecKeyRef> public_key(SecKeyCopyPublicKey(private_key));
  if (!public_key) {
    DLOG(ERROR) << "failed to get public key for credential id "
                << base::HexEncode(credential_id.data(), credential_id.size());
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }

  base::Optional<AuthenticatorData> authenticator_data = MakeAuthenticatorData(
      request_.client_data_hash(), std::move(credential_id), public_key);
  if (!authenticator_data) {
    DLOG(ERROR) << "MakeAuthenticatorData failed";
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }
  base::Optional<std::vector<uint8_t>> signature = GenerateSignature(
      *authenticator_data, request_.client_data_hash(), private_key);
  if (!signature) {
    DLOG(ERROR) << "GenerateSignature failed";
    std::move(callback_).Run(CtapDeviceResponseCode::kCtap2ErrOther,
                             base::nullopt);
    return;
  }
  std::move(callback_).Run(
      CtapDeviceResponseCode::kSuccess,
      AuthenticatorGetAssertionResponse(std::move(*authenticator_data),
                                        std::move(*signature)));
}

}  // namespace mac
}  // namespace fido
}  // namespace device
