// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_whitelist.h"

#include <cstdlib>

#include "net/cert/x509_certificate.h"

namespace net {

namespace {

// clang-format off
const uint8_t kBuiltinWhitelistHashes[][crypto::kSHA256Length] = {
    { 0x00 }
};

const PublicKeyWhitelist kBuiltinWhitelist[] = {
    { { 0x00 },
      kBuiltinWhitelistHashes, 0},
};
// clang-format on
const size_t kBuiltinWhitelistSize = 0;

const PublicKeyWhitelist* g_whitelist = kBuiltinWhitelist;
size_t g_whitelist_size = kBuiltinWhitelistSize;

// Comparator to compare a SHA256HashValue with a uint8_t array containing a
// raw SHA-256 hash.
// Return value follows memcmp semantics.
int CompareHashValueToRawHash(const void* key, const void* element) {
  const SHA256HashValue* search_key =
      reinterpret_cast<const SHA256HashValue*>(key);
  return memcmp(search_key->data, element, sizeof(search_key->data));
}

}  // namespace

bool IsNonWhitelistedCertificate(const X509Certificate& cert,
                                 const HashValueVector& public_key_hashes) {
  if (g_whitelist_size == 0)
    return false;
  for (size_t i = 0; i < g_whitelist_size; ++i) {
    for (const auto& hash : public_key_hashes) {
      if (hash.tag != HASH_VALUE_SHA256)
        continue;
      if (memcmp(hash.data(), g_whitelist[i].public_key,
                 crypto::kSHA256Length) != 0) {
        continue;
      }
      const SHA256HashValue leaf_hash =
          X509Certificate::CalculateFingerprint256(cert.os_cert_handle());
      void* result = bsearch(&leaf_hash, g_whitelist[i].whitelist,
                             g_whitelist[i].whitelist_size,
                             crypto::kSHA256Length, CompareHashValueToRawHash);
      if (result == nullptr)
        return true;
      return false;
    }
  }
  return false;
}

void SetCertificateWhitelistForTesting(const PublicKeyWhitelist* whitelist,
                                       size_t whitelist_size) {
  if (whitelist == nullptr || whitelist_size == 0) {
    g_whitelist = kBuiltinWhitelist;
    g_whitelist_size = kBuiltinWhitelistSize;
    return;
  }

  g_whitelist = whitelist;
  g_whitelist_size = whitelist_size;
}

}  // namespace net
