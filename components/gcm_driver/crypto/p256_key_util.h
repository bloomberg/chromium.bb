// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_P256_KEY_UTIL_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_P256_KEY_UTIL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/strings/string_piece.h"

namespace gcm {

// Creates a new key pair for key exchanges using elliptic-curve Diffie-
// Hellman using the NIST P-256 curve. Returns whether the key pair could be
// created successfully, and was written to the out arguments.
//
// The |out_private_key| will be an ASN.1-encoded PKCS#8 EncryptedPrivateKeyInfo
// block, |out_public_key| an octet string in uncompressed form per SEC1 2.3.3.
bool CreateP256KeyPair(std::string* out_private_key,
                       std::string* out_public_key) WARN_UNUSED_RESULT;

// Computes the shared secret between |private_key| and |peer_public_key|. The
// |public_key| associated with the |private_key| is necessary for NSS. Returns
// whether the secret could be computed, and was written to the out argument.
//
// The |private_key| must be an ASN.1-encoded PKCS#8 EncryptedPrivateKeyInfo
// block. This is legacy from the NSS implementation.
//
// The |peer_public_key| must be an octet string in uncompressed form per
// SEC1 2.3.3.
bool ComputeSharedP256Secret(const base::StringPiece& private_key,
                             const base::StringPiece& peer_public_key,
                             std::string* out_shared_secret) WARN_UNUSED_RESULT;

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_P256_KEY_UTIL_H_
