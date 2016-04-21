// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/p256_key_util.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/logging.h"
#include "crypto/ec_private_key.h"

namespace gcm {

namespace {

// The first byte in an uncompressed P-256 point per SEC1 2.3.3.
const char kUncompressedPointForm = 0x04;

// A P-256 field element consists of 32 bytes.
const size_t kFieldBytes = 32;

// A P-256 point in uncompressed form consists of 0x04 (to denote that the point
// is uncompressed per SEC1 2.3.3) followed by two, 32-byte field elements.
const size_t kUncompressedPointBytes = 1 + 2 * kFieldBytes;

}  // namespace

bool CreateP256KeyPair(std::string* out_private_key,
                       std::string* out_public_key_x509,
                       std::string* out_public_key) {
  DCHECK(out_private_key);
  DCHECK(out_public_key);

  std::unique_ptr<crypto::ECPrivateKey> key_pair(
      crypto::ECPrivateKey::Create());
  if (!key_pair.get()) {
    DLOG(ERROR) << "Unable to generate a new P-256 key pair.";
    return false;
  }

  std::vector<uint8_t> private_key;

  // Export the encrypted private key with an empty password. This is not done
  // to provide any security, but rather to achieve a consistent private key
  // storage between the BoringSSL and NSS implementations.
  if (!key_pair->ExportEncryptedPrivateKey(
          "" /* password */, 1 /* iteration */, &private_key)) {
    DLOG(ERROR) << "Unable to export the private key.";
    return false;
  }

  std::string candidate_public_key;

  // ECPrivateKey::ExportRawPublicKey() returns the EC point in the uncompressed
  // point format, but does not include the leading byte of value 0x04 that
  // indicates usage of uncompressed points, per SEC1 2.3.3.
  if (!key_pair->ExportRawPublicKey(&candidate_public_key) ||
      candidate_public_key.size() != 2 * kFieldBytes) {
    DLOG(ERROR) << "Unable to export the public key.";
    return false;
  }

  std::vector<uint8_t> public_key_x509;

  // Export the public key to an X.509 SubjectPublicKeyInfo for enabling NSS to
  // import the key material when computing a shared secret.
  if (!key_pair->ExportPublicKey(&public_key_x509)) {
    DLOG(ERROR) << "Unable to export the public key as an X.509 "
                << "SubjectPublicKeyInfo block.";
    return false;
  }

  out_private_key->assign(reinterpret_cast<const char*>(private_key.data()),
                          private_key.size());
  out_public_key_x509->assign(
      reinterpret_cast<const char*>(public_key_x509.data()),
      public_key_x509.size());

  // Concatenate the leading 0x04 byte and the two uncompressed points.
  out_public_key->reserve(kUncompressedPointBytes);
  out_public_key->push_back(kUncompressedPointForm);
  out_public_key->append(candidate_public_key);

  return true;
}

}  // namespace gcm
