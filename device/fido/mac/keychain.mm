// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/keychain.h"

#import <Foundation/Foundation.h>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "device/fido/mac/credential_metadata.h"

namespace device {
namespace fido {
namespace mac {

static API_AVAILABLE(macos(10.12.2)) Keychain* g_keychain_instance_override =
    nullptr;

// static
Keychain& Keychain::GetInstance() {
  if (g_keychain_instance_override) {
    return *g_keychain_instance_override;
  }
  static base::NoDestructor<Keychain> k;
  return *k;
}

// static
void Keychain::SetInstanceOverride(Keychain* keychain) {
  CHECK(!g_keychain_instance_override);
  g_keychain_instance_override = keychain;
}

// static
void Keychain::ClearInstanceOverride() {
  CHECK(g_keychain_instance_override);
  g_keychain_instance_override = nullptr;
}

Keychain::Keychain() = default;
Keychain::~Keychain() = default;

base::ScopedCFTypeRef<SecKeyRef> Keychain::KeyCreateRandomKey(
    CFDictionaryRef params,
    CFErrorRef* error) {
  return base::ScopedCFTypeRef<SecKeyRef>(SecKeyCreateRandomKey(params, error));
}

base::ScopedCFTypeRef<CFDataRef> Keychain::KeyCreateSignature(
    SecKeyRef key,
    SecKeyAlgorithm algorithm,
    CFDataRef data,
    CFErrorRef* error) {
  return base::ScopedCFTypeRef<CFDataRef>(
      SecKeyCreateSignature(key, algorithm, data, error));
}

base::ScopedCFTypeRef<SecKeyRef> Keychain::KeyCopyPublicKey(SecKeyRef key) {
  return base::ScopedCFTypeRef<SecKeyRef>(SecKeyCopyPublicKey(key));
}

OSStatus Keychain::ItemCopyMatching(CFDictionaryRef query, CFTypeRef* result) {
  return SecItemCopyMatching(query, result);
}

OSStatus Keychain::ItemDelete(CFDictionaryRef query) {
  return SecItemDelete(query);
}

Credential::Credential(base::ScopedCFTypeRef<SecKeyRef> private_key_,
                       std::vector<uint8_t> credential_id_)
    : private_key(std::move(private_key_)),
      credential_id(std::move(credential_id_)) {}
Credential::~Credential() = default;
Credential::Credential(Credential&& other) = default;
Credential& Credential::operator=(Credential&& other) = default;

base::Optional<Credential> FindCredentialInKeychain(
    const std::string& keychain_access_group,
    const std::string& metadata_secret,
    const std::string& rp_id,
    const std::set<std::vector<uint8_t>>& credential_id_filter,
    LAContext* authentication_context) {
  base::Optional<std::string> encoded_rp_id =
      CredentialMetadata::EncodeRpId(metadata_secret, rp_id);
  if (!encoded_rp_id)
    return base::nullopt;

  base::ScopedCFTypeRef<CFMutableDictionaryRef> query(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0, nullptr, nullptr));
  CFDictionarySetValue(query, kSecClass, kSecClassKey);
  CFDictionarySetValue(query, kSecAttrAccessGroup,
                       base::SysUTF8ToNSString(keychain_access_group));
  CFDictionarySetValue(query, kSecAttrLabel,
                       base::SysUTF8ToNSString(*encoded_rp_id));
  if (authentication_context) {
    CFDictionarySetValue(query, kSecUseAuthenticationContext,
                         authentication_context);
  }
  CFDictionarySetValue(query, kSecReturnRef, @YES);
  CFDictionarySetValue(query, kSecReturnAttributes, @YES);
  CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitAll);

  base::ScopedCFTypeRef<CFArrayRef> keychain_items;
  OSStatus status = Keychain::GetInstance().ItemCopyMatching(
      query, reinterpret_cast<CFTypeRef*>(keychain_items.InitializeInto()));
  if (status == errSecItemNotFound) {
    // No credentials for the RP.
    return base::nullopt;
  }
  if (status != errSecSuccess) {
    OSSTATUS_DLOG(ERROR, status) << "SecItemCopyMatching failed";
    return base::nullopt;
  }

  // Credentials for the RP exist. Find a match.
  std::vector<uint8_t> credential_id;
  base::ScopedCFTypeRef<SecKeyRef> private_key(nil);
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
    if (credential_id_filter.empty() ||
        base::ContainsKey(credential_id_filter, cid)) {
      private_key.reset(key, base::scoped_policy::RETAIN);
      credential_id = std::move(cid);
      break;
    }
  }
  if (private_key == nil) {
    DVLOG(1) << "no allowed credential found";
    return base::nullopt;
  }
  return Credential(std::move(private_key), std::move(credential_id));
}

}  // namespace mac
}  // namespace fido
}  // namespace device
