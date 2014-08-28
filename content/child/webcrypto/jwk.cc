// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jwk.h"

#include <algorithm>
#include <functional>
#include <map>

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
// Web Crypto Key keyUsage        <-- JWK use, key_ops + input usage_mask
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
// The 'algorithm', 'extractable', and 'usage_mask' input parameters
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
// usage_mask
//   The input usage_mask must be a strict subset of the interpreted JWK use
//   value, else it is judged inconsistent. In all cases the input usage_mask
//   is used as the final usage_mask.
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

class JwkWriter {
 public:
  JwkWriter(const std::string& algorithm,
            bool extractable,
            blink::WebCryptoKeyUsageMask usage_mask,
            const std::string& kty) {
    dict_.SetString("alg", algorithm);
    dict_.Set("key_ops", CreateJwkKeyOpsFromWebCryptoUsages(usage_mask));
    dict_.SetBoolean("ext", extractable);
    dict_.SetString("kty", kty);
  }

  void Set(const std::string& key, const std::string& value) {
    dict_.SetString(key, value);
  }

  void SetBase64Encoded(const std::string& key, const CryptoData& value) {
    dict_.SetString(key,
                    Base64EncodeUrlSafe(base::StringPiece(
                        reinterpret_cast<const char*>(value.bytes()),
                        value.byte_length())));
  }

  void ToBytes(std::vector<uint8_t>* utf8_bytes) {
    std::string json;
    base::JSONWriter::Write(&dict_, &json);
    utf8_bytes->assign(json.begin(), json.end());
  }

 private:
  base::DictionaryValue dict_;
};

// Extracts the required string property with key |path| from |dict| and saves
// the result to |*result|. If the property does not exist or is not a string,
// returns an error.
Status GetJwkString(base::DictionaryValue* dict,
                    const std::string& path,
                    std::string* result) {
  base::Value* value = NULL;
  if (!dict->Get(path, &value))
    return Status::ErrorJwkPropertyMissing(path);
  if (!value->GetAsString(result))
    return Status::ErrorJwkPropertyWrongType(path, "string");
  return Status::Success();
}

// Extracts the optional string property with key |path| from |dict| and saves
// the result to |*result| if it was found. If the property exists and is not a
// string, returns an error. Otherwise returns success, and sets
// |*property_exists| if it was found.
Status GetOptionalJwkString(base::DictionaryValue* dict,
                            const std::string& path,
                            std::string* result,
                            bool* property_exists) {
  *property_exists = false;
  base::Value* value = NULL;
  if (!dict->Get(path, &value))
    return Status::Success();

  if (!value->GetAsString(result))
    return Status::ErrorJwkPropertyWrongType(path, "string");

  *property_exists = true;
  return Status::Success();
}

// Extracts the optional array property with key |path| from |dict| and saves
// the result to |*result| if it was found. If the property exists and is not an
// array, returns an error. Otherwise returns success, and sets
// |*property_exists| if it was found. Note that |*result| is owned by |dict|.
Status GetOptionalJwkList(base::DictionaryValue* dict,
                          const std::string& path,
                          base::ListValue** result,
                          bool* property_exists) {
  *property_exists = false;
  base::Value* value = NULL;
  if (!dict->Get(path, &value))
    return Status::Success();

  if (!value->GetAsList(result))
    return Status::ErrorJwkPropertyWrongType(path, "list");

  *property_exists = true;
  return Status::Success();
}

// Extracts the required string property with key |path| from |dict| and saves
// the base64url-decoded bytes to |*result|. If the property does not exist or
// is not a string, or could not be base64url-decoded, returns an error.
Status GetJwkBytes(base::DictionaryValue* dict,
                   const std::string& path,
                   std::string* result) {
  std::string base64_string;
  Status status = GetJwkString(dict, path, &base64_string);
  if (status.IsError())
    return status;

  if (!Base64DecodeUrlSafe(base64_string, result))
    return Status::ErrorJwkBase64Decode(path);

  return Status::Success();
}

// Extracts the required base64url property, which is interpreted as being a
// big-endian unsigned integer.
Status GetJwkBigInteger(base::DictionaryValue* dict,
                        const std::string& path,
                        std::string* result) {
  Status status = GetJwkBytes(dict, path, result);
  if (status.IsError())
    return status;

  if (result->empty())
    return Status::ErrorJwkEmptyBigInteger(path);

  // The JWA spec says that "The octet sequence MUST utilize the minimum number
  // of octets to represent the value." This means there shouldn't be any
  // leading zeros.
  if (result->size() > 1 && (*result)[0] == 0)
    return Status::ErrorJwkBigIntegerHasLeadingZero(path);

  return Status::Success();
}

// Extracts the optional boolean property with key |path| from |dict| and saves
// the result to |*result| if it was found. If the property exists and is not a
// boolean, returns an error. Otherwise returns success, and sets
// |*property_exists| if it was found.
Status GetOptionalJwkBool(base::DictionaryValue* dict,
                          const std::string& path,
                          bool* result,
                          bool* property_exists) {
  *property_exists = false;
  base::Value* value = NULL;
  if (!dict->Get(path, &value))
    return Status::Success();

  if (!value->GetAsBoolean(result))
    return Status::ErrorJwkPropertyWrongType(path, "boolean");

  *property_exists = true;
  return Status::Success();
}

Status VerifyExt(base::DictionaryValue* dict, bool expected_extractable) {
  // JWK "ext" (optional) --> extractable parameter
  bool jwk_ext_value = false;
  bool has_jwk_ext;
  Status status = GetOptionalJwkBool(dict, "ext", &jwk_ext_value, &has_jwk_ext);
  if (status.IsError())
    return status;
  if (has_jwk_ext && expected_extractable && !jwk_ext_value)
    return Status::ErrorJwkExtInconsistent();
  return Status::Success();
}

Status VerifyUsages(base::DictionaryValue* dict,
                    blink::WebCryptoKeyUsageMask expected_usage_mask) {
  // JWK "key_ops" (optional) --> usage_mask parameter
  base::ListValue* jwk_key_ops_value = NULL;
  bool has_jwk_key_ops;
  Status status =
      GetOptionalJwkList(dict, "key_ops", &jwk_key_ops_value, &has_jwk_key_ops);
  if (status.IsError())
    return status;
  blink::WebCryptoKeyUsageMask jwk_key_ops_mask = 0;
  if (has_jwk_key_ops) {
    status =
        GetWebCryptoUsagesFromJwkKeyOps(jwk_key_ops_value, &jwk_key_ops_mask);
    if (status.IsError())
      return status;
    // The input usage_mask must be a subset of jwk_key_ops_mask.
    if (!ContainsKeyUsages(jwk_key_ops_mask, expected_usage_mask))
      return Status::ErrorJwkKeyopsInconsistent();
  }

  // JWK "use" (optional) --> usage_mask parameter
  std::string jwk_use_value;
  bool has_jwk_use;
  status = GetOptionalJwkString(dict, "use", &jwk_use_value, &has_jwk_use);
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
    // The input usage_mask must be a subset of jwk_use_mask.
    if (!ContainsKeyUsages(jwk_use_mask, expected_usage_mask))
      return Status::ErrorJwkUseInconsistent();
  }

  // If both 'key_ops' and 'use' are present, ensure they are consistent.
  if (has_jwk_key_ops && has_jwk_use &&
      !ContainsKeyUsages(jwk_use_mask, jwk_key_ops_mask))
    return Status::ErrorJwkUseAndKeyopsInconsistent();

  return Status::Success();
}

Status VerifyAlg(base::DictionaryValue* dict,
                 const std::string& expected_algorithm) {
  // JWK "alg" --> algorithm parameter
  bool has_jwk_alg;
  std::string jwk_alg_value;
  Status status =
      GetOptionalJwkString(dict, "alg", &jwk_alg_value, &has_jwk_alg);
  if (status.IsError())
    return status;

  if (has_jwk_alg && jwk_alg_value != expected_algorithm)
    return Status::ErrorJwkAlgorithmInconsistent();

  return Status::Success();
}

Status ParseJwkCommon(const CryptoData& bytes,
                      bool expected_extractable,
                      blink::WebCryptoKeyUsageMask expected_usage_mask,
                      std::string* kty,
                      scoped_ptr<base::DictionaryValue>* dict) {
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
  dict->reset(dict_value);

  // JWK "kty". Exit early if this required JWK parameter is missing.
  Status status = GetJwkString(dict_value, "kty", kty);
  if (status.IsError())
    return status;

  status = VerifyExt(dict_value, expected_extractable);
  if (status.IsError())
    return status;

  status = VerifyUsages(dict_value, expected_usage_mask);
  if (status.IsError())
    return status;

  return Status::Success();
}

Status ReadSecretKeyNoExpectedAlg(
    const CryptoData& key_data,
    bool expected_extractable,
    blink::WebCryptoKeyUsageMask expected_usage_mask,
    std::vector<uint8_t>* raw_key_data,
    scoped_ptr<base::DictionaryValue>* dict) {
  if (!key_data.byte_length())
    return Status::ErrorImportEmptyKeyData();

  std::string kty;
  Status status = ParseJwkCommon(
      key_data, expected_extractable, expected_usage_mask, &kty, dict);
  if (status.IsError())
    return status;

  if (kty != "oct")
    return Status::ErrorJwkUnexpectedKty("oct");

  std::string jwk_k_value;
  status = GetJwkBytes(dict->get(), "k", &jwk_k_value);
  if (status.IsError())
    return status;
  raw_key_data->assign(jwk_k_value.begin(), jwk_k_value.end());

  return Status::Success();
}

}  // namespace

void WriteSecretKeyJwk(const CryptoData& raw_key_data,
                       const std::string& algorithm,
                       bool extractable,
                       blink::WebCryptoKeyUsageMask usage_mask,
                       std::vector<uint8_t>* jwk_key_data) {
  JwkWriter writer(algorithm, extractable, usage_mask, "oct");
  writer.SetBase64Encoded("k", raw_key_data);
  writer.ToBytes(jwk_key_data);
}

Status ReadSecretKeyJwk(const CryptoData& key_data,
                        const std::string& expected_algorithm,
                        bool expected_extractable,
                        blink::WebCryptoKeyUsageMask expected_usage_mask,
                        std::vector<uint8_t>* raw_key_data) {
  scoped_ptr<base::DictionaryValue> dict;
  Status status = ReadSecretKeyNoExpectedAlg(
      key_data, expected_extractable, expected_usage_mask, raw_key_data, &dict);
  if (status.IsError())
    return status;
  return VerifyAlg(dict.get(), expected_algorithm);
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
                           blink::WebCryptoKeyUsageMask expected_usage_mask,
                           std::vector<uint8_t>* raw_key_data) {
  scoped_ptr<base::DictionaryValue> dict;
  Status status = ReadSecretKeyNoExpectedAlg(
      key_data, expected_extractable, expected_usage_mask, raw_key_data, &dict);
  if (status.IsError())
    return status;

  bool has_jwk_alg;
  std::string jwk_alg;
  status = GetOptionalJwkString(dict.get(), "alg", &jwk_alg, &has_jwk_alg);
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
                          blink::WebCryptoKeyUsageMask usage_mask,
                          std::vector<uint8_t>* jwk_key_data) {
  JwkWriter writer(algorithm, extractable, usage_mask, "RSA");
  writer.SetBase64Encoded("n", n);
  writer.SetBase64Encoded("e", e);
  writer.ToBytes(jwk_key_data);
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
                           blink::WebCryptoKeyUsageMask usage_mask,
                           std::vector<uint8_t>* jwk_key_data) {
  JwkWriter writer(algorithm, extractable, usage_mask, "RSA");

  writer.SetBase64Encoded("n", n);
  writer.SetBase64Encoded("e", e);
  writer.SetBase64Encoded("d", d);
  // Although these are "optional" in the JWA, WebCrypto spec requires them to
  // be emitted.
  writer.SetBase64Encoded("p", p);
  writer.SetBase64Encoded("q", q);
  writer.SetBase64Encoded("dp", dp);
  writer.SetBase64Encoded("dq", dq);
  writer.SetBase64Encoded("qi", qi);
  writer.ToBytes(jwk_key_data);
}

JwkRsaInfo::JwkRsaInfo() : is_private_key(false) {
}

JwkRsaInfo::~JwkRsaInfo() {
}

Status ReadRsaKeyJwk(const CryptoData& key_data,
                     const std::string& expected_algorithm,
                     bool expected_extractable,
                     blink::WebCryptoKeyUsageMask expected_usage_mask,
                     JwkRsaInfo* result) {
  if (!key_data.byte_length())
    return Status::ErrorImportEmptyKeyData();

  scoped_ptr<base::DictionaryValue> dict;
  std::string kty;
  Status status = ParseJwkCommon(
      key_data, expected_extractable, expected_usage_mask, &kty, &dict);
  if (status.IsError())
    return status;

  status = VerifyAlg(dict.get(), expected_algorithm);
  if (status.IsError())
    return status;

  if (kty != "RSA")
    return Status::ErrorJwkUnexpectedKty("RSA");

  // An RSA public key must have an "n" (modulus) and an "e" (exponent) entry
  // in the JWK, while an RSA private key must have those, plus at least a "d"
  // (private exponent) entry.
  // See http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-18,
  // section 6.3.
  status = GetJwkBigInteger(dict.get(), "n", &result->n);
  if (status.IsError())
    return status;
  status = GetJwkBigInteger(dict.get(), "e", &result->e);
  if (status.IsError())
    return status;

  result->is_private_key = dict->HasKey("d");
  if (!result->is_private_key)
    return Status::Success();

  status = GetJwkBigInteger(dict.get(), "d", &result->d);
  if (status.IsError())
    return status;

  // The "p", "q", "dp", "dq", and "qi" properties are optional in the JWA
  // spec. However they are required by Chromium's WebCrypto implementation.

  status = GetJwkBigInteger(dict.get(), "p", &result->p);
  if (status.IsError())
    return status;

  status = GetJwkBigInteger(dict.get(), "q", &result->q);
  if (status.IsError())
    return status;

  status = GetJwkBigInteger(dict.get(), "dp", &result->dp);
  if (status.IsError())
    return status;

  status = GetJwkBigInteger(dict.get(), "dq", &result->dq);
  if (status.IsError())
    return status;

  status = GetJwkBigInteger(dict.get(), "qi", &result->qi);
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
