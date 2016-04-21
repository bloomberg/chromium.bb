// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/ec.h>
#include <openssl/ecdh.h>
#include <openssl/evp.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "components/gcm_driver/crypto/p256_key_util.h"
#include "crypto/ec_private_key.h"
#include "crypto/scoped_openssl_types.h"

namespace gcm {

namespace {

// A P-256 field element consists of 32 bytes.
const size_t kFieldBytes = 32;

}  // namespace

bool ComputeSharedP256Secret(const base::StringPiece& private_key,
                             const base::StringPiece& public_key_x509,
                             const base::StringPiece& peer_public_key,
                             std::string* out_shared_secret) {
  DCHECK(out_shared_secret);

  std::unique_ptr<crypto::ECPrivateKey> local_key_pair(
      crypto::ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(
          "" /* no password */,
          std::vector<uint8_t>(private_key.data(),
                               private_key.data() + private_key.size()),
          std::vector<uint8_t>(
              public_key_x509.data(),
              public_key_x509.data() + public_key_x509.size())));

  if (!local_key_pair) {
    DLOG(ERROR) << "Unable to create the local key pair.";
    return false;
  }

  crypto::ScopedEC_KEY ec_private_key(
      EVP_PKEY_get1_EC_KEY(local_key_pair->key()));

  if (!ec_private_key || !EC_KEY_check_key(ec_private_key.get())) {
    DLOG(ERROR) << "The private key is invalid.";
    return false;
  }

  crypto::ScopedEC_POINT point(
      EC_POINT_new(EC_KEY_get0_group(ec_private_key.get())));

  if (!point ||
      !EC_POINT_oct2point(EC_KEY_get0_group(ec_private_key.get()), point.get(),
                                            reinterpret_cast<const uint8_t*>(
                                                peer_public_key.data()),
                                            peer_public_key.size(), nullptr)) {
    DLOG(ERROR) << "Can't convert peer public value to curve point.";
    return false;
  }

  uint8_t result[kFieldBytes];
  if (ECDH_compute_key(result, sizeof(result), point.get(),
                       ec_private_key.get(), nullptr) != sizeof(result)) {
    DLOG(ERROR) << "Unable to compute the ECDH shared secret.";
    return false;
  }

  out_shared_secret->assign(reinterpret_cast<char*>(result), sizeof(result));
  return true;
}

}  // namespace gcm
