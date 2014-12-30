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
#include "third_party/WebKit/public/platform/WebCrypto.h"

namespace content {

namespace webcrypto {

class CryptoData;
class Status;

// Helper class for parsing a JWK from JSON.
//
// This primarily exists to ensure strict enforcement of the JWK schema, as the
// type and presence of particular members is security relevant. For example,
// GetString() will ensure a given JSON member is present and is a string type,
// and will fail if these conditions aren't met.
//
// Users of JwkReader must call Init() successfully before any other method can
// be called.
class JwkReader {
 public:
  JwkReader();
  ~JwkReader();

  // Initializes a JWK reader by parsing the JSON |bytes|. To succeed, the JWK
  // must:
  //   * Have "kty" matching |expected_kty|
  //   * Have "ext" compatible with |expected_extractable|
  //   * Have usages ("use", "key_ops") compatible with |expected_usages|
  //   * Have an "alg" matching |expected_alg|
  //
  // NOTE: If |expected_alg| is empty, then the test on "alg" is skipped.
  Status Init(const CryptoData& bytes,
              bool expected_extractable,
              blink::WebCryptoKeyUsageMask expected_usages,
              const std::string& expected_kty,
              const std::string& expected_alg);

  // Returns true if the member |member_name| is present.
  bool HasMember(const std::string& member_name) const;

  // Extracts the required string member |member_name| and saves the result to
  // |*result|. If the member does not exist or is not a string, returns an
  // error.
  Status GetString(const std::string& member_name, std::string* result) const;

  // Extracts the optional string member |member_name| and saves the result to
  // |*result| if it was found. If the member exists and is not a string,
  // returns an error. Otherwise returns success, and sets |*member_exists| if
  // it was found.
  Status GetOptionalString(const std::string& member_name,
                           std::string* result,
                           bool* member_exists) const;

  // Extracts the optional array member |member_name| and saves the result to
  // |*result| if it was found. If the member exists and is not an array,
  // returns an error. Otherwise returns success, and sets |*member_exists| if
  // it was found.
  //
  // NOTE: |*result| is owned by the JwkReader.
  Status GetOptionalList(const std::string& member_name,
                         base::ListValue** result,
                         bool* member_exists) const;

  // Extracts the required string member |member_name| and saves the
  // base64url-decoded bytes to |*result|. If the member does not exist or is
  // not a string, or could not be base64url-decoded, returns an error.
  Status GetBytes(const std::string& member_name, std::string* result) const;

  // Extracts the required base64url member, which is interpreted as being a
  // big-endian unsigned integer.
  //
  // Sequences that contain leading zeros will be rejected.
  Status GetBigInteger(const std::string& member_name,
                       std::string* result) const;

  // Extracts the optional boolean member |member_name| and saves the result to
  // |*result| if it was found. If the member exists and is not a boolean,
  // returns an error. Otherwise returns success, and sets |*member_exists| if
  // it was found.
  Status GetOptionalBool(const std::string& member_name,
                         bool* result,
                         bool* member_exists) const;

  // Gets the optional algorithm ("alg") string.
  Status GetAlg(std::string* alg, bool* has_alg) const;

  // Checks if the "alg" member matches |expected_alg|.
  Status VerifyAlg(const std::string& expected_alg) const;

 private:
  scoped_ptr<base::DictionaryValue> dict_;
};

// Helper class for building the JSON for a JWK.
class JwkWriter {
 public:
  // Initializes a writer, and sets the standard JWK members as indicated.
  // |algorithm| is optional, and is only written if the provided |algorithm| is
  // non-empty.
  JwkWriter(const std::string& algorithm,
            bool extractable,
            blink::WebCryptoKeyUsageMask usages,
            const std::string& kty);

  // Sets a string member |member_name| to |value|.
  void SetString(const std::string& member_name, const std::string& value);

  // Sets a bytes member |value| to |value| by base64 url-safe encoding it.
  void SetBytes(const std::string& member_name, const CryptoData& value);

  // Flattens the JWK to JSON (UTF-8 encoded if necessary, however in practice
  // it will be ASCII).
  void ToJson(std::vector<uint8_t>* utf8_bytes) const;

 private:
  base::DictionaryValue dict_;
};

// Writes a JWK-formatted symmetric key to |jwk_key_data|.
//  * raw_key_data: The actual key data
//  * algorithm: The JWK algorithm name (i.e. "alg")
//  * extractable: The JWK extractability (i.e. "ext")
//  * usages: The JWK usages (i.e. "key_ops")
void WriteSecretKeyJwk(const CryptoData& raw_key_data,
                       const std::string& algorithm,
                       bool extractable,
                       blink::WebCryptoKeyUsageMask usages,
                       std::vector<uint8_t>* jwk_key_data);

// Parses a UTF-8 encoded JWK (key_data), and extracts the key material to
// |*raw_key_data|. Returns Status::Success() on success, otherwise an error.
// In order for this to succeed:
//   * expected_alg must match the JWK's "alg", if present.
//   * expected_extractable must be consistent with the JWK's "ext", if
//     present.
//   * expected_usages must be a subset of the JWK's "key_ops" if present.
Status ReadSecretKeyJwk(const CryptoData& key_data,
                        const std::string& expected_alg,
                        bool expected_extractable,
                        blink::WebCryptoKeyUsageMask expected_usages,
                        std::vector<uint8_t>* raw_key_data);

// Creates an AES algorithm name for the given key size (in bytes). For
// instance "A128CBC" is the result of suffix="CBC", keylen_bytes=16.
std::string MakeJwkAesAlgorithmName(const std::string& suffix,
                                    unsigned int keylen_bytes);

// This is very similar to ReadSecretKeyJwk(), except instead of specifying an
// absolute "expected_alg", the suffix for an AES algorithm name is given
// (See MakeJwkAesAlgorithmName() for an explanation of what the suffix is).
//
// This is because the algorithm name for AES keys is dependent on the length
// of the key. This function expects key lengths to be either 128, 192, or 256
// bits.
Status ReadAesSecretKeyJwk(const CryptoData& key_data,
                           const std::string& algorithm_name_suffix,
                           bool expected_extractable,
                           blink::WebCryptoKeyUsageMask expected_usages,
                           std::vector<uint8_t>* raw_key_data);

// Writes a JWK-formated RSA public key and saves the result to
// |*jwk_key_data|.
void WriteRsaPublicKeyJwk(const CryptoData& n,
                          const CryptoData& e,
                          const std::string& algorithm,
                          bool extractable,
                          blink::WebCryptoKeyUsageMask usages,
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
                           blink::WebCryptoKeyUsageMask usages,
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
//   * expected_alg must match the JWK's "alg", if present.
//   * expected_extractable must be consistent with the JWK's "ext", if
//     present.
//   * expected_usages must be a subset of the JWK's "key_ops" if present.
Status ReadRsaKeyJwk(const CryptoData& key_data,
                     const std::string& expected_alg,
                     bool expected_extractable,
                     blink::WebCryptoKeyUsageMask expected_usages,
                     JwkRsaInfo* result);

const char* GetJwkHmacAlgorithmName(blink::WebCryptoAlgorithmId hash);

// This decodes JWK's flavor of base64 encoding, as described by:
// https://tools.ietf.org/html/draft-ietf-jose-json-web-signature-36#section-2
//
// In essence it is RFC 4648 'base64url' encoding where padding is omitted.
CONTENT_EXPORT bool Base64DecodeUrlSafe(const std::string& input,
                                        std::string* output);

// Encodes |input| using JWK's flavor of base64 encoding. See the description
// above for details.
CONTENT_EXPORT std::string Base64EncodeUrlSafe(const base::StringPiece& input);
CONTENT_EXPORT std::string Base64EncodeUrlSafe(
    const std::vector<uint8_t>& input);

// Converts a JWK "key_ops" array to the corresponding WebCrypto usages. Used by
// testing.
CONTENT_EXPORT Status
GetWebCryptoUsagesFromJwkKeyOpsForTest(const base::ListValue* key_ops,
                                       blink::WebCryptoKeyUsageMask* usages);

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_JWK_H_
