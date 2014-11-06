// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_OPENSSL_UTIL_OPENSSL_H_
#define CONTENT_CHILD_WEBCRYPTO_OPENSSL_UTIL_OPENSSL_H_

#include <string>
#include <vector>

#include <openssl/ossl_typ.h>

#include "crypto/scoped_openssl_types.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"

namespace content {

namespace webcrypto {

class CryptoData;
class Status;

// The values of these constants correspond with the "enc" parameter of
// EVP_CipherInit_ex(), do not change.
enum EncryptOrDecrypt { DECRYPT=0, ENCRYPT=1 };

const EVP_MD* GetDigest(blink::WebCryptoAlgorithmId id);

// Does either encryption or decryption for an AEAD algorithm.
//   * |mode| controls whether encryption or decryption is done
//   * |aead_alg| the algorithm (for instance AES-GCM)
//   * |buffer| where the ciphertext or plaintext is written to.
Status AeadEncryptDecrypt(EncryptOrDecrypt mode,
                          const std::vector<uint8_t>& raw_key,
                          const CryptoData& data,
                          unsigned int tag_length_bytes,
                          const CryptoData& iv,
                          const CryptoData& additional_data,
                          const EVP_AEAD* aead_alg,
                          std::vector<uint8_t>* buffer);

// Creates a WebCrypto public key given an EVP_PKEY. This step includes
// exporting the key to SPKI format, for use by serialization later.
Status CreateWebCryptoPublicKey(
    crypto::ScopedEVP_PKEY public_key,
    const blink::WebCryptoKeyAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key);

// Creates a WebCrypto private key given an EVP_PKEY. This step includes
// exporting the key to PKCS8 format, for use by serialization later.
Status CreateWebCryptoPrivateKey(
    crypto::ScopedEVP_PKEY private_key,
    const blink::WebCryptoKeyAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoKey* key);

// Imports SPKI bytes to an EVP_PKEY for a public key. The resulting asymmetric
// key may be invalid, and should be verified using something like
// RSA_check_key(). The only validation performed by this function is to ensure
// the key type matched |expected_pkey_id|.
Status ImportUnverifiedPkeyFromSpki(const CryptoData& key_data,
                                    int expected_pkey_id,
                                    crypto::ScopedEVP_PKEY* pkey);

// Imports PKCS8 bytes to an EVP_PKEY for a private key. The resulting
// asymmetric key may be invalid, and should be verified using something like
// RSA_check_key(). The only validation performed by this function is to ensure
// the key type matched |expected_pkey_id|.
Status ImportUnverifiedPkeyFromPkcs8(const CryptoData& key_data,
                                     int expected_pkey_id,
                                     crypto::ScopedEVP_PKEY* pkey);

// Allocates a new BIGNUM given a std::string big-endian representation.
BIGNUM* CreateBIGNUM(const std::string& n);

// Converts a BIGNUM to a big endian byte array.
std::vector<uint8_t> BIGNUMToVector(const BIGNUM* n);

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_OPENSSL_UTIL_OPENSSL_H_
