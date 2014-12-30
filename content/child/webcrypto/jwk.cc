// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/jwk.h"

#include <set>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"

// TODO(eroman): The algorithm-specific logic in this file for AES and RSA
// should be moved into the corresponding AlgorithmImplementation file. It
// exists in this file to avoid duplication between OpenSSL and NSS
// implementations.

// JSON Web Key Format (JWK) is defined by:
// http://tools.ietf.org/html/draft-ietf-jose-json-web-key
//
// A JWK is a simple JSON dictionary with the following members:
// - "kty" (Key Type) Parameter, REQUIRED
// - <kty-specific parameters, see below>, REQUIRED
// - "use" (Key Use) OPTIONAL
// - "key_ops" (Key Operations) OPTIONAL
// - "alg" (Algorithm) OPTIONAL
// - "ext" (Key Exportability), OPTIONAL
// (all other entries are ignored)
//
// The <kty-specific parameters> are defined by the JWA spec:
// http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms

namespace content {

namespace webcrypto {

namespace {

// Web Crypto equivalent usage mask for JWK 'use' = 'enc'.
const blink::WebCryptoKeyUsageMask kJwkEncUsage =
    blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt |
    blink::WebCryptoKeyUsageWrapKey | blink::WebCryptoKeyUsageUnwrapKey;
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

struct JwkToWebCryptoUsageMapping {
  const char* const jwk_key_op;
  const blink::WebCryptoKeyUsage webcrypto_usage;
};

// Keep this ordered the same as WebCrypto's "recognized key usage
// values". While this is not required for spec compliance,
// it makes the ordering of key_ops match that of WebCrypto's Key.usages.
const JwkToWebCryptoUsageMapping kJwkWebCryptoUsageMap[] = {
    {"encrypt", blink::WebCryptoKeyUsageEncrypt},
    {"decrypt", blink::WebCryptoKeyUsageDecrypt},
    {"sign", blink::WebCryptoKeyUsageSign},
    {"verify", blink::WebCryptoKeyUsageVerify},
    {"deriveKey", blink::WebCryptoKeyUsageDeriveKey},
    {"deriveBits", blink::WebCryptoKeyUsageDeriveBits},
    {"wrapKey", blink::WebCryptoKeyUsageWrapKey},
    {"unwrapKey", blink::WebCryptoKeyUsageUnwrapKey}};

bool JwkKeyOpToWebCryptoUsage(const std::string& key_op,
                              blink::WebCryptoKeyUsage* usage) {
  for (size_t i = 0; i < arraysize(kJwkWebCryptoUsageMap); ++i) {
    if (kJwkWebCryptoUsageMap[i].jwk_key_op == key_op) {
      *usage = kJwkWebCryptoUsageMap[i].webcrypto_usage;
      return true;
    }
  }
  return false;
}

// Creates a JWK key_ops list from a Web Crypto usage mask.
scoped_ptr<base::ListValue> CreateJwkKeyOpsFromWebCryptoUsages(
    blink::WebCryptoKeyUsageMask usages) {
  scoped_ptr<base::ListValue> jwk_key_ops(new base::ListValue());
  for (size_t i = 0; i < arraysize(kJwkWebCryptoUsageMap); ++i) {
    if (usages & kJwkWebCryptoUsageMap[i].webcrypto_usage)
      jwk_key_ops->AppendString(kJwkWebCryptoUsageMap[i].jwk_key_op);
  }
  return jwk_key_ops.Pass();
}

// Composes a Web Crypto usage mask from an array of JWK key_ops values.
Status GetWebCryptoUsagesFromJwkKeyOps(const base::ListValue* key_ops,
                                       blink::WebCryptoKeyUsageMask* usages) {
  // This set keeps track of all unrecognized key_ops values.
  std::set<std::string> unrecognized_usages;

  *usages = 0;
  for (size_t i = 0; i < key_ops->GetSize(); ++i) {
    std::string key_op;
    if (!key_ops->GetString(i, &key_op)) {
      return Status::ErrorJwkMemberWrongType(
          base::StringPrintf("key_ops[%d]", static_cast<int>(i)), "string");
    }

    blink::WebCryptoKeyUsage usage;
    if (JwkKeyOpToWebCryptoUsage(key_op, &usage)) {
      // Ensure there are no duplicate usages.
      if (*usages & usage)
        return Status::ErrorJwkDuplicateKeyOps();
      *usages |= usage;
    }

    // Reaching here means the usage was unrecognized. Such usages are skipped
    // over, however they are kept track of in a set to ensure there were no
    // duplicates.
    if (!unrecognized_usages.insert(key_op).second)
      return Status::ErrorJwkDuplicateKeyOps();
  }
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
    return Status::ErrorJwkMemberMissing(member_name);
  if (!value->GetAsString(result))
    return Status::ErrorJwkMemberWrongType(member_name, "string");
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
    return Status::ErrorJwkMemberWrongType(member_name, "string");

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
    return Status::ErrorJwkMemberWrongType(member_name, "list");

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
    return Status::ErrorJwkMemberWrongType(member_name, "boolean");

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
  if (!algorithm.empty())
    dict_.SetString("alg", algorithm);
  dict_.Set("key_ops", CreateJwkKeyOpsFromWebCryptoUsages(usages).release());
  dict_.SetBoolean("ext", extractable);
  dict_.SetString("kty", kty);
}

void JwkWriter::SetString(const std::string& member_name,
                          const std::string& value) {
  dict_.SetString(member_name, value);
}

void JwkWriter::SetBytes(const std::string& member_name,
                         const CryptoData& value) {
  dict_.SetString(member_name, Base64EncodeUrlSafe(base::StringPiece(
                                   reinterpret_cast<const char*>(value.bytes()),
                                   value.byte_length())));
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
  Status status = jwk->Init(key_data, expected_extractable, expected_usages,
                            "oct", std::string());
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
  Status status = jwk.Init(key_data, expected_extractable, expected_usages,
                           "RSA", expected_alg);
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

bool Base64DecodeUrlSafe(const std::string& input, std::string* output) {
  // The JSON web signature spec specifically says that padding is omitted.
  if (input.find_first_of("+/=") != std::string::npos)
    return false;

  std::string base64_encoded_text(input);
  std::replace(base64_encoded_text.begin(), base64_encoded_text.end(), '-',
               '+');
  std::replace(base64_encoded_text.begin(), base64_encoded_text.end(), '_',
               '/');
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

Status GetWebCryptoUsagesFromJwkKeyOpsForTest(
    const base::ListValue* key_ops,
    blink::WebCryptoKeyUsageMask* usages) {
  return GetWebCryptoUsagesFromJwkKeyOps(key_ops, usages);
}

}  // namespace webcrypto

}  // namespace content
