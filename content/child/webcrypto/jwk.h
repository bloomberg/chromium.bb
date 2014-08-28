// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_JWK_H_
#define CONTENT_CHILD_WEBCRYPTO_JWK_H_

#include <stdint.h>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCrypto.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

namespace content {

namespace webcrypto {

class CryptoData;
class Status;

// Writes a JWK-formatted symmetric key to |jwk_key_data|.
//  * raw_key_data: The actual key data
//  * algorithm: The JWK algorithm name (i.e. "alg")
//  * extractable: The JWK extractability (i.e. "ext")
//  * usage_mask: The JWK usages (i.e. "key_ops")
void WriteSecretKeyJwk(const CryptoData& raw_key_data,
                       const std::string& algorithm,
                       bool extractable,
                       blink::WebCryptoKeyUsageMask usage_mask,
                       std::vector<uint8_t>* jwk_key_data);

// Parses a UTF-8 encoded JWK (key_data), and extracts the key material to
// |*raw_key_data|. Returns Status::Success() on success, otherwise an error.
// In order for this to succeed:
//   * expected_algorithm must match the JWK's "alg", if present.
//   * expected_extractable must be consistent with the JWK's "ext", if
//     present.
//   * expected_usage_mask must be a subset of the JWK's "key_ops" if present.
Status ReadSecretKeyJwk(const CryptoData& key_data,
                        const std::string& expected_algorithm,
                        bool expected_extractable,
                        blink::WebCryptoKeyUsageMask expected_usage_mask,
                        std::vector<uint8_t>* raw_key_data);

// Creates an AES algorithm name for the given key size (in bytes). For
// instance "A128CBC" is the result of suffix="CBC", keylen_bytes=16.
std::string MakeJwkAesAlgorithmName(const std::string& suffix,
                                    unsigned int keylen_bytes);

// This is very similar to ReadSecretKeyJwk(), except instead of specifying an
// absolut "expected_algorithm", the suffix for an AES algorithm name is given
// (See MakeJwkAesAlgorithmName() for an explanation of what the suffix is).
//
// This is because the algorithm name for AES keys is dependent on the length
// of the key. This function expects key lengths to be either 128, 192, or 256
// bits.
Status ReadAesSecretKeyJwk(const CryptoData& key_data,
                           const std::string& algorithm_name_suffix,
                           bool expected_extractable,
                           blink::WebCryptoKeyUsageMask expected_usage_mask,
                           std::vector<uint8_t>* raw_key_data);

// Writes a JWK-formated RSA public key and saves the result to
// |*jwk_key_data|.
void WriteRsaPublicKeyJwk(const CryptoData& n,
                          const CryptoData& e,
                          const std::string& algorithm,
                          bool extractable,
                          blink::WebCryptoKeyUsageMask usage_mask,
                          std::vector<uint8_t>* jwk_key_data);

// Writes a JWK-formated RSA private key and saves the result to
// |*jwk_key_data|.
void WriteRsaPrivateKeyJwk(const CryptoData& n,
                           const CryptoData& e,
                           const CryptoData& d,
                           const CryptoData& p,
                           const CryptoData& q,
                           const CryptoData& dp,
                           const CryptoData& dq,
                           const CryptoData& qi,
                           const std::string& algorithm,
                           bool extractable,
                           blink::WebCryptoKeyUsageMask usage_mask,
                           std::vector<uint8_t>* jwk_key_data);

// Describes the RSA components for a parsed key. The names of the properties
// correspond with those from the JWK spec. Note that Chromium's WebCrypto
// implementation does not support multi-primes, so there is no parsed field
// for othinfo.
struct JwkRsaInfo {
  JwkRsaInfo();
  ~JwkRsaInfo();

  bool is_private_key;
  std::string n;
  std::string e;
  std::string d;
  std::string p;
  std::string q;
  std::string dp;
  std::string dq;
  std::string qi;
};

// Parses a UTF-8 encoded JWK (key_data), and extracts the RSA components to
// |*result|. Returns Status::Success() on success, otherwise an error.
// In order for this to succeed:
//   * expected_algorithm must match the JWK's "alg", if present.
//   * expected_extractable must be consistent with the JWK's "ext", if
//     present.
//   * expected_usage_mask must be a subset of the JWK's "key_ops" if present.
Status ReadRsaKeyJwk(const CryptoData& key_data,
                     const std::string& expected_algorithm,
                     bool expected_extractable,
                     blink::WebCryptoKeyUsageMask expected_usage_mask,
                     JwkRsaInfo* result);

const char* GetJwkHmacAlgorithmName(blink::WebCryptoAlgorithmId hash);

// This function decodes unpadded 'base64url' encoded data, as described in
// RFC4648 (http://www.ietf.org/rfc/rfc4648.txt) Section 5.
CONTENT_EXPORT bool Base64DecodeUrlSafe(const std::string& input,
                                        std::string* output);

// Returns an unpadded 'base64url' encoding of the input data, the opposite of
// Base64DecodeUrlSafe() above.
CONTENT_EXPORT std::string Base64EncodeUrlSafe(const base::StringPiece& input);
CONTENT_EXPORT std::string Base64EncodeUrlSafe(
    const std::vector<uint8_t>& input);

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_JWK_H_
