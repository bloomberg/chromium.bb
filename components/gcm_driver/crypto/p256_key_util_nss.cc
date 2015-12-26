// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/p256_key_util.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "crypto/ec_private_key.h"
#include "crypto/scoped_nss_types.h"

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

  scoped_ptr<crypto::ECPrivateKey> local_key_pair(
      crypto::ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(
          "" /* no password */,
          std::vector<uint8_t>(
              private_key.data(), private_key.data() + private_key.size()),
          std::vector<uint8_t>(
              public_key_x509.data(),
              public_key_x509.data() + public_key_x509.size())));

  if (!local_key_pair) {
    DLOG(ERROR) << "Unable to create the local key pair.";
    return false;
  }

  SECKEYPublicKey public_key_peer;
  memset(&public_key_peer, 0, sizeof(public_key_peer));

  public_key_peer.keyType = ecKey;
  // Both sides of a ECDH key exchange need to use the same EC params.
  public_key_peer.u.ec.DEREncodedParams.len =
      local_key_pair->public_key()->u.ec.DEREncodedParams.len;
  public_key_peer.u.ec.DEREncodedParams.data =
      local_key_pair->public_key()->u.ec.DEREncodedParams.data;

  public_key_peer.u.ec.publicValue.type = siBuffer;
  public_key_peer.u.ec.publicValue.data =
      reinterpret_cast<uint8_t*>(const_cast<char*>(peer_public_key.data()));
  public_key_peer.u.ec.publicValue.len = peer_public_key.size();

  // The NSS function performing ECDH key exchange is PK11_PubDeriveWithKDF.
  // As this function is used for SSL/TLS's ECDH key exchanges it has many
  // arguments, most of which are not required. Key derivation function CKD_NULL
  // is used because the return value of |CalculateSharedKey| is the actual ECDH
  // shared key, not any derived keys from it.
  crypto::ScopedPK11SymKey premaster_secret(
      PK11_PubDeriveWithKDF(
          local_key_pair->key(),
          &public_key_peer,
          PR_FALSE /* isSender */,
          nullptr /* randomA */,
          nullptr /* randomB */,
          CKM_ECDH1_DERIVE,
          CKM_GENERIC_SECRET_KEY_GEN,
          CKA_DERIVE,
          0 /* keySize */,
          CKD_NULL /* kdf */,
          nullptr /* sharedData */,
          nullptr /* wincx */));

  if (!premaster_secret) {
    DLOG(ERROR) << "Unable to derive the ECDH shared key.";
    return false;
  }

  if (PK11_ExtractKeyValue(premaster_secret.get()) != SECSuccess) {
    DLOG(ERROR) << "Unable to extract the raw ECDH shared secret.";
    return false;
  }

  SECItem* key_data = PK11_GetKeyData(premaster_secret.get());
  if (!key_data || !key_data->data || key_data->len != kFieldBytes) {
    DLOG(ERROR) << "The raw ECDH shared secret is invalid.";
    return false;
  }

  out_shared_secret->assign(
      reinterpret_cast<char*>(key_data->data), key_data->len);
  return true;
}

}  // namespace gcm
