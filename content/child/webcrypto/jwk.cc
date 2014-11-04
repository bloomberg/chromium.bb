// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/jwk.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"

// TODO(eroman): The algorithm-specific logic in this file for AES and RSA
// should be moved into the corresponding AlgorithmImplementation file. It
// exists in this file to avoid duplication between OpenSSL and NSS
// implementations.

// JSON Web Key Format (JWK)
// http://tools.ietf.org/html/draft-ietf-jose-json-web-key-21
//
// A JWK is a simple JSON dictionary with the following entries
// - "kty" (Key Type) Parameter, REQUIRED
// - <kty-specific parameters, see below>, REQUIRED
// - "use" (Key Use) Parameter, OPTIONAL
// - "key_ops" (Key Operations) Parameter, OPTIONAL
// - "alg" (Algorithm) Parameter, OPTIONAL
// - "ext" (Key Exportability), OPTIONAL
// (all other entries are ignored)
//
// OPTIONAL here means that this code does not require the entry to be present
// in the incoming JWK, because the method input parameters contain similar
// information. If the optional JWK entry is present, it will be validated
// against the corresponding input parameter for consistency and combined with
// it according to rules defined below.
//
// Input 'key_data' contains the JWK. To build a Web Crypto Key, the JWK
// values are parsed out and combined with the method input parameters to
// build a Web Crypto Key:
// Web Crypto Key type            <-- (deduced)
// Web Crypto Key extractable     <-- JWK ext + input extractable
// Web Crypto Key algorithm       <-- JWK alg + input algorithm
// Web Crypto Key keyUsage        <-- JWK use, key_ops + input usages
// Web Crypto Key keying material <-- kty-specific parameters
//
// Values for each JWK entry are case-sensitive and defined in
// http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-18.
// Note that not all values specified by JOSE are handled by this code. Only
// handled values are listed.
// - kty (Key Type)
//   +-------+--------------------------------------------------------------+
//   | "RSA" | RSA [RFC3447]                                                |
//   | "oct" | Octet sequence (used to represent symmetric keys)            |
//   +-------+--------------------------------------------------------------+
//
// - key_ops (Key Use Details)
//   The key_ops field is an array that contains one or more strings from
//   the table below, and describes the operations for which this key may be
//   used.
//   +-------+--------------------------------------------------------------+
//   | "encrypt"    | encrypt operations                                    |
//   | "decrypt"    | decrypt operations                                    |
//   | "sign"       | sign (MAC) operations                                 |
//   | "verify"     | verify (MAC) operations                               |
//   | "wrapKey"    | key wrap                                              |
//   | "unwrapKey"  | key unwrap                                            |
//   | "deriveKey"  | key derivation                                        |
//   | "deriveBits" | key derivation                                        |
//   +-------+--------------------------------------------------------------+
//
// - use (Key Use)
//   The use field contains a single entry from the table below.
//   +-------+--------------------------------------------------------------+
//   | "sig"     | equivalent to key_ops of [sign, verify]                  |
//   | "enc"     | equivalent to key_ops of [encrypt, decrypt, wrapKey,     |
//   |           | unwrapKey, deriveKey, deriveBits]                        |
//   +-------+--------------------------------------------------------------+
//
//   NOTE: If both "use" and "key_ops" JWK members are present, the usages
//   specified by them MUST be consistent.  In particular, the "use" value
//   "sig" corresponds to "sign" and/or "verify".  The "use" value "enc"
//   corresponds to all other values defined above.  If "key_ops" values
//   corresponding to both "sig" and "enc" "use" values are present, the "use"
//   member SHOULD NOT be present, and if present, its value MUST NOT be
//   either "sig" or "enc".
//
// - ext (Key Exportability)
//   +-------+--------------------------------------------------------------+
//   | true  | Key may be exported from the trusted environment             |
//   | false | Key cannot exit the trusted environment                      |
//   +-------+--------------------------------------------------------------+
//
// - alg (Algorithm)
//   See http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-18
//   +--------------+-------------------------------------------------------+
//   | Digital Signature or MAC Algorithm                                   |
//   +--------------+-------------------------------------------------------+
//   | "HS1"        | HMAC using SHA-1 hash algorithm                       |
//   | "HS256"      | HMAC using SHA-256 hash algorithm                     |
//   | "HS384"      | HMAC using SHA-384 hash algorithm                     |
//   | "HS512"      | HMAC using SHA-512 hash algorithm                     |
//   | "RS1"        | RSASSA using SHA-1 hash algorithm
//   | "RS256"      | RSASSA using SHA-256 hash algorithm                   |
//   | "RS384"      | RSASSA using SHA-384 hash algorithm                   |
//   | "RS512"      | RSASSA using SHA-512 hash algorithm                   |
//   +--------------+-------------------------------------------------------|
//   | Key Management Algorithm                                             |
//   +--------------+-------------------------------------------------------+
//   | "RSA-OAEP"   | RSAES using Optimal Asymmetric Encryption Padding     |
//   |              | (OAEP) [RFC3447], with the default parameters         |
//   |              | specified by RFC3447 in Section A.2.1                 |
//   | "A128KW"     | Advanced Encryption Standard (AES) Key Wrap Algorithm |
//   |              | [RFC3394] using 128 bit keys                          |
//   | "A192KW"     | AES Key Wrap Algorithm using 192 bit keys             |
//   | "A256KW"     | AES Key Wrap Algorithm using 256 bit keys             |
//   | "A128GCM"    | AES in Galois/Counter Mode (GCM) [NIST.800-38D] using |
//   |              | 128 bit keys                                          |
//   | "A192GCM"    | AES GCM using 192 bit keys                            |
//   | "A256GCM"    | AES GCM using 256 bit keys                            |
//   | "A128CBC"    | AES in Cipher Block Chaining Mode (CBC) with PKCS #5  |
//   |              | padding [NIST.800-38A]                                |
//   | "A192CBC"    | AES CBC using 192 bit keys                            |
//   | "A256CBC"    | AES CBC using 256 bit keys                            |
//   +--------------+-------------------------------------------------------+
//
// kty-specific parameters
// The value of kty determines the type and content of the keying material
// carried in the JWK to be imported.
// // - kty == "oct" (symmetric or other raw key)
//   +-------+--------------------------------------------------------------+
//   | "k"   | Contains the value of the symmetric (or other single-valued) |
//   |       | key.  It is represented as the base64url encoding of the     |
//   |       | octet sequence containing the key value.                     |
//   +-------+--------------------------------------------------------------+
// - kty == "RSA" (RSA public key)
//   +-------+--------------------------------------------------------------+
//   | "n"   | Contains the modulus value for the RSA public key.  It is    |
//   |       | represented as the base64url encoding of the value's         |
//   |       | unsigned big endian representation as an octet sequence.     |
//   +-------+--------------------------------------------------------------+
//   | "e"   | Contains the exponent value for the RSA public key.  It is   |
//   |       | represented as the base64url encoding of the value's         |
//   |       | unsigned big endian representation as an octet sequence.     |
//   +-------+--------------------------------------------------------------+
// - If key == "RSA" and the "d" parameter is present then it is a private key.
//   All the parameters above for public keys apply, as well as the following.
//   (Note that except for "d", all of these are optional):
//   +-------+--------------------------------------------------------------+
//   | "d"   | Contains the private exponent value for the RSA private key. |
//   |       | It is represented as the base64url encoding of the value's   |
//   |       | unsigned big endian representation as an octet sequence.     |
//   +-------+--------------------------------------------------------------+
//   | "p"   | Contains the first prime factor value for the RSA private    |
//   |       | key.  It is represented as the base64url encoding of the     |
//   |       | value's                                                      |
//   |       | unsigned big endian representation as an octet sequence.     |
//   +-------+--------------------------------------------------------------+
//   | "q"   | Contains the second prime factor value for the RSA private   |
//   |       | key.  It is represented as the base64url encoding of the     |
//   |       | value's unsigned big endian representation as an octet       |
//   |       | sequence.                                                    |
//   +-------+--------------------------------------------------------------+
//   | "dp"  | Contains the first factor CRT exponent value for the RSA     |
//   |       | private key.  It is represented as the base64url encoding of |
//   |       | the value's unsigned big endian representation as an octet   |
//   |       | sequence.                                                    |
//   +-------+--------------------------------------------------------------+
//   | "dq"  | Contains the second factor CRT exponent value for the RSA    |
//   |       | private key.  It is represented as the base64url encoding of |
//   |       | the value's unsigned big endian representation as an octet   |
//   |       | sequence.                                                    |
//   +-------+--------------------------------------------------------------+
//   | "dq"  | Contains the first CRT coefficient value for the RSA private |
//   |       | key.  It is represented as the base64url encoding of the     |
//   |       | value's unsigned big endian representation as an octet       |
//   |       | sequence.                                                    |
//   +-------+--------------------------------------------------------------+
//
// Consistency and conflict resolution
// The 'algorithm', 'extractable', and 'usages' input parameters
// may be different than the corresponding values inside the JWK. The Web
// Crypto spec says that if a JWK value is present but is inconsistent with
// the input value, it is an error and the operation must fail. If no
// inconsistency is found then the input parameters are used.
//
// algorithm
//   If the JWK algorithm is provided, it must match the web crypto input
//   algorithm (both the algorithm ID and inner hash if applicable).
//
// extractable
//   If the JWK ext field is true but the input parameter is false, make the
//   Web Crypto Key non-extractable. Conversely, if the JWK ext field is
//   false but the input parameter is true, it is an inconsistency. If both
//   are true or both are false, use that value.
//
// usages
//   The input usages must be a strict subset of the interpreted JWK use
//   value, else it is judged inconsistent. In all cases the input usages
//   is used as the final usages.
//

namespace content {

namespace webcrypto {

namespace {

// Web Crypto equivalent usage mask for JWK 'use' = 'enc'.
const blink::WebCryptoKeyUsageMask kJwkEncUsage =
    blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt |
    blink::WebCryptoKeyUsageWrapKey | blink::WebCryptoKeyUsageUnwrapKey |
    blink::WebCryptoKeyUsageDeriveKey | blink::WebCryptoKeyUsageDeriveBits;
// Web Crypto equivalent usage mask for JWK 'use' = 'sig'.
const blink::WebCryptoKeyUsageMask kJwkSigUsage =
    blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify;

// Checks that the "ext" member of the JWK is consistent with
// "expected_extractable".
Status VerifyExt(const JwkReader& jwk, bool expected_extractable) {
  // JWK "ext" (optional) --> extractable parameter
  bool jwk_ext_value = false;
  bool has_jwk_ext;
  Status status = jwk.GetOptionalBool("ext", &jwk_ext_value, &has_jwk_ext);
  if (status.IsError())
    return status;
  if (has_jwk_ext && expected_extractable && !jwk_ext_value)
    return Status::ErrorJwkExtInconsistent();
  return Status::Success();
}

// Checks that the usages ("use" and "key_ops") of the JWK is consistent with
// "expected_usages".
Status VerifyUsages(const JwkReader& jwk,
                    blink::WebCryptoKeyUsageMask expected_usages) {
  // JWK "key_ops" (optional) --> usages parameter
  base::ListValue* jwk_key_ops_value = NULL;
  bool has_jwk_key_ops;
  Status status =
      jwk.GetOptionalList("key_ops", &jwk_key_ops_value, &has_jwk_key_ops);
  if (status.IsError())
    return status;
  blink::WebCryptoKeyUsageMask jwk_key_ops_mask = 0;
  if (has_jwk_key_ops) {
    status =
        GetWebCryptoUsagesFromJwkKeyOps(jwk_key_ops_value, &jwk_key_ops_mask);
    if (status.IsError())
      return status;
    // The input usages must be a subset of jwk_key_ops_mask.
    if (!ContainsKeyUsages(jwk_key_ops_mask, expected_usages))
      return Status::ErrorJwkKeyopsInconsistent();
  }

  // JWK "use" (optional) --> usages parameter
  std::string jwk_use_value;
  bool has_jwk_use;
  status = jwk.GetOptionalString("use", &jwk_use_value, &has_jwk_use);
  if (status.IsError())
    return status;
  blink::WebCryptoKeyUsageMask jwk_use_mask = 0;
  if (has_jwk_use) {
    if (jwk_use_value == "enc")
      jwk_use_mask = kJwkEncUsage;
    else if (jwk_use_value == "sig")
      jwk_use_mask = kJwkSigUsage;
    else
      return Status::ErrorJwkUnrecognizedUse();
    // The input usages must be a subset of jwk_use_mask.
    if (!ContainsKeyUsages(jwk_use_mask, expected_usages))
      return Status::ErrorJwkUseInconsistent();
  }

  // If both 'key_ops' and 'use' are present, ensure they are consistent.
  if (has_jwk_key_ops && has_jwk_use &&
      !ContainsKeyUsages(jwk_use_mask, jwk_key_ops_mask))
    return Status::ErrorJwkUseAndKeyopsInconsistent();

  return Status::Success();
}

}  // namespace

JwkReader::JwkReader() {
}

JwkReader::~JwkReader() {
}

Status JwkReader::Init(const CryptoData& bytes,
                       bool expected_extractable,
                       blink::WebCryptoKeyUsageMask expected_usages,
                       const std::string& expected_kty,
                       const std::string& expected_alg) {
  if (!bytes.byte_length())
    return Status::ErrorImportEmptyKeyData();

  // Parse the incoming JWK JSON.
  base::StringPiece json_string(reinterpret_cast<const char*>(bytes.bytes()),
                                bytes.byte_length());

  scoped_ptr<base::Value> value(base::JSONReader::Read(json_string));
  base::DictionaryValue* dict_value = NULL;

  if (!value.get() || !value->GetAsDictionary(&dict_value) || !dict_value)
    return Status::ErrorJwkNotDictionary();

  // Release |value|, as ownership will be transferred to |dict| via
  // |dict_value|, which points to the same object as |value|.
  ignore_result(value.release());
  dict_.reset(dict_value);

  // JWK "kty". Exit early if this required JWK parameter is missing.
  std::string kty;
  Status status = GetString("kty", &kty);
  if (status.IsError())
    return status;

  if (kty != expected_kty)
    return Status::ErrorJwkUnexpectedKty(expected_kty);

  status = VerifyExt(*this, expected_extractable);
  if (status.IsError())
    return status;

  status = VerifyUsages(*this, expected_usages);
  if (status.IsError())
    return status;

  // Verify the algorithm if an expectation was provided.
  if (!expected_alg.empty()) {
    status = VerifyAlg(expected_alg);
    if (status.IsError())
      return status;
  }

  return Status::Success();
}

bool JwkReader::HasMember(const std::string& member_name) const {
  return dict_->HasKey(member_name);
}

Status JwkReader::GetString(const std::string& member_name,
                            std::string* result) const {
  base::Value* value = NULL;
  if (!dict_->Get(member_name, &value))
    return Status::ErrorJwkPropertyMissing(member_name);
  if (!value->GetAsString(result))
    return Status::ErrorJwkPropertyWrongType(member_name, "string");
  return Status::Success();
}

Status JwkReader::GetOptionalString(const std::string& member_name,
                                    std::string* result,
                                    bool* member_exists) const {
  *member_exists = false;
  base::Value* value = NULL;
  if (!dict_->Get(member_name, &value))
    return Status::Success();

  if (!value->GetAsString(result))
    return Status::ErrorJwkPropertyWrongType(member_name, "string");

  *member_exists = true;
  return Status::Success();
}

Status JwkReader::GetOptionalList(const std::string& member_name,
                                  base::ListValue** result,
                                  bool* member_exists) const {
  *member_exists = false;
  base::Value* value = NULL;
  if (!dict_->Get(member_name, &value))
    return Status::Success();

  if (!value->GetAsList(result))
    return Status::ErrorJwkPropertyWrongType(member_name, "list");

  *member_exists = true;
  return Status::Success();
}

Status JwkReader::GetBytes(const std::string& member_name,
                           std::string* result) const {
  std::string base64_string;
  Status status = GetString(member_name, &base64_string);
  if (status.IsError())
    return status;

  if (!Base64DecodeUrlSafe(base64_string, result))
    return Status::ErrorJwkBase64Decode(member_name);

  return Status::Success();
}

Status JwkReader::GetBigInteger(const std::string& member_name,
                                std::string* result) const {
  Status status = GetBytes(member_name, result);
  if (status.IsError())
    return status;

  if (result->empty())
    return Status::ErrorJwkEmptyBigInteger(member_name);

  // The JWA spec says that "The octet sequence MUST utilize the minimum number
  // of octets to represent the value." This means there shouldn't be any
  // leading zeros.
  if (result->size() > 1 && (*result)[0] == 0)
    return Status::ErrorJwkBigIntegerHasLeadingZero(member_name);

  return Status::Success();
}

Status JwkReader::GetOptionalBool(const std::string& member_name,
                                  bool* result,
                                  bool* member_exists) const {
  *member_exists = false;
  base::Value* value = NULL;
  if (!dict_->Get(member_name, &value))
    return Status::Success();

  if (!value->GetAsBoolean(result))
    return Status::ErrorJwkPropertyWrongType(member_name, "boolean");

  *member_exists = true;
  return Status::Success();
}

Status JwkReader::GetAlg(std::string* alg, bool* has_alg) const {
  return GetOptionalString("alg", alg, has_alg);
}

Status JwkReader::VerifyAlg(const std::string& expected_alg) const {
  bool has_jwk_alg;
  std::string jwk_alg_value;
  Status status = GetAlg(&jwk_alg_value, &has_jwk_alg);
  if (status.IsError())
    return status;

  if (has_jwk_alg && jwk_alg_value != expected_alg)
    return Status::ErrorJwkAlgorithmInconsistent();

  return Status::Success();
}

JwkWriter::JwkWriter(const std::string& algorithm,
                     bool extractable,
                     blink::WebCryptoKeyUsageMask usages,
                     const std::string& kty) {
  dict_.SetString("alg", algorithm);
  dict_.Set("key_ops", CreateJwkKeyOpsFromWebCryptoUsages(usages));
  dict_.SetBoolean("ext", extractable);
  dict_.SetString("kty", kty);
}

void JwkWriter::SetString(const std::string& member_name,
                          const std::string& value) {
  dict_.SetString(member_name, value);
}

void JwkWriter::SetBytes(const std::string& member_name,
                         const CryptoData& value) {
  dict_.SetString(
      member_name,
      Base64EncodeUrlSafe(base::StringPiece(
          reinterpret_cast<const char*>(value.bytes()), value.byte_length())));
}

void JwkWriter::ToJson(std::vector<uint8_t>* utf8_bytes) const {
  std::string json;
  base::JSONWriter::Write(&dict_, &json);
  utf8_bytes->assign(json.begin(), json.end());
}

Status ReadSecretKeyNoExpectedAlg(const CryptoData& key_data,
                                  bool expected_extractable,
                                  blink::WebCryptoKeyUsageMask expected_usages,
                                  std::vector<uint8_t>* raw_key_data,
                                  JwkReader* jwk) {
  Status status = jwk->Init(
      key_data, expected_extractable, expected_usages, "oct", std::string());
  if (status.IsError())
    return status;

  std::string jwk_k_value;
  status = jwk->GetBytes("k", &jwk_k_value);
  if (status.IsError())
    return status;
  raw_key_data->assign(jwk_k_value.begin(), jwk_k_value.end());

  return Status::Success();
}

void WriteSecretKeyJwk(const CryptoData& raw_key_data,
                       const std::string& algorithm,
                       bool extractable,
                       blink::WebCryptoKeyUsageMask usages,
                       std::vector<uint8_t>* jwk_key_data) {
  JwkWriter writer(algorithm, extractable, usages, "oct");
  writer.SetBytes("k", raw_key_data);
  writer.ToJson(jwk_key_data);
}

Status ReadSecretKeyJwk(const CryptoData& key_data,
                        const std::string& expected_alg,
                        bool expected_extractable,
                        blink::WebCryptoKeyUsageMask expected_usages,
                        std::vector<uint8_t>* raw_key_data) {
  JwkReader jwk;
  Status status = ReadSecretKeyNoExpectedAlg(
      key_data, expected_extractable, expected_usages, raw_key_data, &jwk);
  if (status.IsError())
    return status;
  return jwk.VerifyAlg(expected_alg);
}

std::string MakeJwkAesAlgorithmName(const std::string& suffix,
                                    unsigned int keylen_bytes) {
  if (keylen_bytes == 16)
    return std::string("A128") + suffix;
  if (keylen_bytes == 24)
    return std::string("A192") + suffix;
  if (keylen_bytes == 32)
    return std::string("A256") + suffix;
  return std::string();
}

Status ReadAesSecretKeyJwk(const CryptoData& key_data,
                           const std::string& algorithm_name_suffix,
                           bool expected_extractable,
                           blink::WebCryptoKeyUsageMask expected_usages,
                           std::vector<uint8_t>* raw_key_data) {
  JwkReader jwk;
  Status status = ReadSecretKeyNoExpectedAlg(
      key_data, expected_extractable, expected_usages, raw_key_data, &jwk);
  if (status.IsError())
    return status;

  bool has_jwk_alg;
  std::string jwk_alg;
  status = jwk.GetAlg(&jwk_alg, &has_jwk_alg);
  if (status.IsError())
    return status;

  if (has_jwk_alg) {
    std::string expected_algorithm_name =
        MakeJwkAesAlgorithmName(algorithm_name_suffix, raw_key_data->size());

    if (jwk_alg != expected_algorithm_name) {
      // Give a different error message if the key length was wrong.
      if (jwk_alg == MakeJwkAesAlgorithmName(algorithm_name_suffix, 16) ||
          jwk_alg == MakeJwkAesAlgorithmName(algorithm_name_suffix, 24) ||
          jwk_alg == MakeJwkAesAlgorithmName(algorithm_name_suffix, 32)) {
        return Status::ErrorJwkIncorrectKeyLength();
      }
      return Status::ErrorJwkAlgorithmInconsistent();
    }
  }

  return Status::Success();
}

// Writes an RSA public key to a JWK dictionary
void WriteRsaPublicKeyJwk(const CryptoData& n,
                          const CryptoData& e,
                          const std::string& algorithm,
                          bool extractable,
                          blink::WebCryptoKeyUsageMask usages,
                          std::vector<uint8_t>* jwk_key_data) {
  JwkWriter writer(algorithm, extractable, usages, "RSA");
  writer.SetBytes("n", n);
  writer.SetBytes("e", e);
  writer.ToJson(jwk_key_data);
}

// Writes an RSA private key to a JWK dictionary
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
                           std::vector<uint8_t>* jwk_key_data) {
  JwkWriter writer(algorithm, extractable, usages, "RSA");

  writer.SetBytes("n", n);
  writer.SetBytes("e", e);
  writer.SetBytes("d", d);
  // Although these are "optional" in the JWA, WebCrypto spec requires them to
  // be emitted.
  writer.SetBytes("p", p);
  writer.SetBytes("q", q);
  writer.SetBytes("dp", dp);
  writer.SetBytes("dq", dq);
  writer.SetBytes("qi", qi);
  writer.ToJson(jwk_key_data);
}

JwkRsaInfo::JwkRsaInfo() : is_private_key(false) {
}

JwkRsaInfo::~JwkRsaInfo() {
}

Status ReadRsaKeyJwk(const CryptoData& key_data,
                     const std::string& expected_alg,
                     bool expected_extractable,
                     blink::WebCryptoKeyUsageMask expected_usages,
                     JwkRsaInfo* result) {
  JwkReader jwk;
  Status status = jwk.Init(
      key_data, expected_extractable, expected_usages, "RSA", expected_alg);
  if (status.IsError())
    return status;

  // An RSA public key must have an "n" (modulus) and an "e" (exponent) entry
  // in the JWK, while an RSA private key must have those, plus at least a "d"
  // (private exponent) entry.
  // See http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-18,
  // section 6.3.
  status = jwk.GetBigInteger("n", &result->n);
  if (status.IsError())
    return status;
  status = jwk.GetBigInteger("e", &result->e);
  if (status.IsError())
    return status;

  result->is_private_key = jwk.HasMember("d");
  if (!result->is_private_key)
    return Status::Success();

  status = jwk.GetBigInteger("d", &result->d);
  if (status.IsError())
    return status;

  // The "p", "q", "dp", "dq", and "qi" properties are optional in the JWA
  // spec. However they are required by Chromium's WebCrypto implementation.

  status = jwk.GetBigInteger("p", &result->p);
  if (status.IsError())
    return status;

  status = jwk.GetBigInteger("q", &result->q);
  if (status.IsError())
    return status;

  status = jwk.GetBigInteger("dp", &result->dp);
  if (status.IsError())
    return status;

  status = jwk.GetBigInteger("dq", &result->dq);
  if (status.IsError())
    return status;

  status = jwk.GetBigInteger("qi", &result->qi);
  if (status.IsError())
    return status;

  return Status::Success();
}

const char* GetJwkHmacAlgorithmName(blink::WebCryptoAlgorithmId hash) {
  switch (hash) {
    case blink::WebCryptoAlgorithmIdSha1:
      return "HS1";
    case blink::WebCryptoAlgorithmIdSha256:
      return "HS256";
    case blink::WebCryptoAlgorithmIdSha384:
      return "HS384";
    case blink::WebCryptoAlgorithmIdSha512:
      return "HS512";
    default:
      return NULL;
  }
}

// TODO(eroman): This accepts invalid inputs. http://crbug.com/378034
bool Base64DecodeUrlSafe(const std::string& input, std::string* output) {
  std::string base64_encoded_text(input);
  std::replace(
      base64_encoded_text.begin(), base64_encoded_text.end(), '-', '+');
  std::replace(
      base64_encoded_text.begin(), base64_encoded_text.end(), '_', '/');
  base64_encoded_text.append((4 - base64_encoded_text.size() % 4) % 4, '=');
  return base::Base64Decode(base64_encoded_text, output);
}

std::string Base64EncodeUrlSafe(const base::StringPiece& input) {
  std::string output;
  base::Base64Encode(input, &output);
  std::replace(output.begin(), output.end(), '+', '-');
  std::replace(output.begin(), output.end(), '/', '_');
  output.erase(std::remove(output.begin(), output.end(), '='), output.end());
  return output;
}

std::string Base64EncodeUrlSafe(const std::vector<uint8_t>& input) {
  const base::StringPiece string_piece(
      reinterpret_cast<const char*>(vector_as_array(&input)), input.size());
  return Base64EncodeUrlSafe(string_piece);
}

}  // namespace webcrypto

}  // namespace content
