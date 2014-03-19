// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <functional>
#include <map>
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/strings/stringprintf.h"
#include "content/child/webcrypto/crypto_data.h"
#include "content/child/webcrypto/platform_crypto.h"
#include "content/child/webcrypto/shared_crypto.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

// Web Crypto equivalent usage mask for JWK 'use' = 'enc'.
// TODO(padolph): Add 'deriveBits' once supported by Blink.
const blink::WebCryptoKeyUsageMask kJwkEncUsage =
    blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt |
    blink::WebCryptoKeyUsageWrapKey | blink::WebCryptoKeyUsageUnwrapKey |
    blink::WebCryptoKeyUsageDeriveKey;
// Web Crypto equivalent usage mask for JWK 'use' = 'sig'.
const blink::WebCryptoKeyUsageMask kJwkSigUsage =
    blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify;

typedef blink::WebCryptoAlgorithm (*AlgorithmCreationFunc)();

class JwkAlgorithmInfo {
 public:
  JwkAlgorithmInfo()
      : creation_func_(NULL),
        required_key_length_bytes_(NO_KEY_SIZE_REQUIREMENT) {}

  explicit JwkAlgorithmInfo(AlgorithmCreationFunc algorithm_creation_func)
      : creation_func_(algorithm_creation_func),
        required_key_length_bytes_(NO_KEY_SIZE_REQUIREMENT) {}

  JwkAlgorithmInfo(AlgorithmCreationFunc algorithm_creation_func,
                   unsigned int required_key_length_bits)
      : creation_func_(algorithm_creation_func),
        required_key_length_bytes_(required_key_length_bits / 8) {
    DCHECK_EQ(0u, required_key_length_bits % 8);
  }

  bool CreateImportAlgorithm(blink::WebCryptoAlgorithm* algorithm) const {
    *algorithm = creation_func_();
    return !algorithm->isNull();
  }

  bool IsInvalidKeyByteLength(size_t byte_length) const {
    if (required_key_length_bytes_ == NO_KEY_SIZE_REQUIREMENT)
      return false;
    return required_key_length_bytes_ != byte_length;
  }

 private:
  enum { NO_KEY_SIZE_REQUIREMENT = UINT_MAX };

  AlgorithmCreationFunc creation_func_;

  // The expected key size for the algorithm or NO_KEY_SIZE_REQUIREMENT.
  unsigned int required_key_length_bytes_;
};

typedef std::map<std::string, JwkAlgorithmInfo> JwkAlgorithmInfoMap;

class JwkAlgorithmRegistry {
 public:
  JwkAlgorithmRegistry() {
    // TODO(eroman):
    // http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-20
    // says HMAC with SHA-2 should have a key size at least as large as the
    // hash output.
    alg_to_info_["HS1"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateHmacImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha1>);
    alg_to_info_["HS256"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateHmacImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha256>);
    alg_to_info_["HS384"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateHmacImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha384>);
    alg_to_info_["HS512"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateHmacImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha512>);
    alg_to_info_["RS256"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateRsaSsaImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha256>);
    alg_to_info_["RS384"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateRsaSsaImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha384>);
    alg_to_info_["RS512"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateRsaSsaImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha512>);
    alg_to_info_["RSA1_5"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm,
                         blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5>);
    alg_to_info_["RSA-OAEP"] =
        JwkAlgorithmInfo(&BindAlgorithmId<CreateRsaOaepImportAlgorithm,
                                          blink::WebCryptoAlgorithmIdSha1>);
    alg_to_info_["A128KW"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesKw>,
        128);
    alg_to_info_["A192KW"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesKw>,
        192);
    alg_to_info_["A256KW"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesKw>,
        256);
    alg_to_info_["A128GCM"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesGcm>,
        128);
    alg_to_info_["A192GCM"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesGcm>,
        192);
    alg_to_info_["A256GCM"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesGcm>,
        256);
    alg_to_info_["A128CBC"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesCbc>,
        128);
    alg_to_info_["A192CBC"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesCbc>,
        192);
    alg_to_info_["A256CBC"] = JwkAlgorithmInfo(
        &BindAlgorithmId<CreateAlgorithm, blink::WebCryptoAlgorithmIdAesCbc>,
        256);
  }

  // Returns NULL if the algorithm name was not registered.
  const JwkAlgorithmInfo* GetAlgorithmInfo(const std::string& jwk_alg) const {
    const JwkAlgorithmInfoMap::const_iterator pos = alg_to_info_.find(jwk_alg);
    if (pos == alg_to_info_.end())
      return NULL;
    return &pos->second;
  }

 private:
  // Binds a WebCryptoAlgorithmId value to a compatible factory function.
  typedef blink::WebCryptoAlgorithm (*FuncWithWebCryptoAlgIdArg)(
      blink::WebCryptoAlgorithmId);
  template <FuncWithWebCryptoAlgIdArg func,
            blink::WebCryptoAlgorithmId algorithm_id>
  static blink::WebCryptoAlgorithm BindAlgorithmId() {
    return func(algorithm_id);
  }

  JwkAlgorithmInfoMap alg_to_info_;
};

base::LazyInstance<JwkAlgorithmRegistry> jwk_alg_registry =
    LAZY_INSTANCE_INITIALIZER;

bool ImportAlgorithmsConsistent(const blink::WebCryptoAlgorithm& alg1,
                                const blink::WebCryptoAlgorithm& alg2) {
  DCHECK(!alg1.isNull());
  DCHECK(!alg2.isNull());
  if (alg1.id() != alg2.id())
    return false;
  if (alg1.paramsType() != alg2.paramsType())
    return false;
  switch (alg1.paramsType()) {
    case blink::WebCryptoAlgorithmParamsTypeNone:
      return true;
    case blink::WebCryptoAlgorithmParamsTypeRsaHashedImportParams:
      return ImportAlgorithmsConsistent(alg1.rsaHashedImportParams()->hash(),
                                        alg2.rsaHashedImportParams()->hash());
    case blink::WebCryptoAlgorithmParamsTypeHmacImportParams:
      return ImportAlgorithmsConsistent(alg1.hmacImportParams()->hash(),
                                        alg2.hmacImportParams()->hash());
    default:
      return false;
  }
}

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

// Returns true if the set bits in b make up a subset of the set bits in a.
bool ContainsKeyUsages(blink::WebCryptoKeyUsageMask a,
                       blink::WebCryptoKeyUsageMask b) {
  return (a & b) == b;
}

// Writes a secret/symmetric key to a JWK dictionary.
void WriteSecretKey(const blink::WebArrayBuffer& raw_key,
                    base::DictionaryValue* jwk_dict) {
  DCHECK(jwk_dict);
  jwk_dict->SetString("kty", "oct");
  // For a secret/symmetric key, the only extra JWK field is 'k', containing the
  // base64url encoding of the raw key.
  DCHECK(!raw_key.isNull());
  DCHECK(raw_key.data());
  DCHECK(raw_key.byteLength());
  unsigned int key_length_bytes = raw_key.byteLength();
  const base::StringPiece key_str(static_cast<const char*>(raw_key.data()),
                                  key_length_bytes);
  jwk_dict->SetString("k", Base64EncodeUrlSafe(key_str));
}

// Writes a Web Crypto usage mask to a JWK dictionary.
void WriteKeyOps(blink::WebCryptoKeyUsageMask key_usages,
                 base::DictionaryValue* jwk_dict) {
  jwk_dict->Set("key_ops", CreateJwkKeyOpsFromWebCryptoUsages(key_usages));
}

// Writes a Web Crypto extractable value to a JWK dictionary.
void WriteExt(bool extractable, base::DictionaryValue* jwk_dict) {
  jwk_dict->SetBoolean("ext", extractable);
}

// Writes a Web Crypto algorithm to a JWK dictionary.
Status WriteAlg(const blink::WebCryptoKeyAlgorithm& algorithm,
                unsigned int raw_key_length_bytes,
                base::DictionaryValue* jwk_dict) {
  switch (algorithm.paramsType()) {
    case blink::WebCryptoKeyAlgorithmParamsTypeAes: {
      const char* aes_prefix = "";
      switch (raw_key_length_bytes) {
        case 16:
          aes_prefix = "A128";
          break;
        case 24:
          aes_prefix = "A192";
          break;
        case 32:
          aes_prefix = "A256";
          break;
        default:
          NOTREACHED();  // bad key length means algorithm was built improperly
          return Status::ErrorUnexpected();
      }
      const char* aes_suffix = "";
      switch (algorithm.id()) {
        case blink::WebCryptoAlgorithmIdAesCbc:
          aes_suffix = "CBC";
          break;
        case blink::WebCryptoAlgorithmIdAesCtr:
          aes_suffix = "CTR";
          break;
        case blink::WebCryptoAlgorithmIdAesGcm:
          aes_suffix = "GCM";
          break;
        case blink::WebCryptoAlgorithmIdAesKw:
          aes_suffix = "KW";
          break;
        default:
          return Status::ErrorUnsupported();
      }
      jwk_dict->SetString("alg",
                          base::StringPrintf("%s%s", aes_prefix, aes_suffix));
      break;
    }
    case blink::WebCryptoKeyAlgorithmParamsTypeHmac: {
      DCHECK(algorithm.hmacParams());
      switch (algorithm.hmacParams()->hash().id()) {
        case blink::WebCryptoAlgorithmIdSha1:
          jwk_dict->SetString("alg", "HS1");
          break;
        case blink::WebCryptoAlgorithmIdSha256:
          jwk_dict->SetString("alg", "HS256");
          break;
        case blink::WebCryptoAlgorithmIdSha384:
          jwk_dict->SetString("alg", "HS384");
          break;
        case blink::WebCryptoAlgorithmIdSha512:
          jwk_dict->SetString("alg", "HS512");
          break;
        default:
          NOTREACHED();
          return Status::ErrorUnexpected();
      }
      break;
    }
    case blink::WebCryptoKeyAlgorithmParamsTypeRsa:
    case blink::WebCryptoKeyAlgorithmParamsTypeRsaHashed:
      // TODO(padolph): Handle RSA key
      return Status::ErrorUnsupported();
    default:
      return Status::ErrorUnsupported();
  }
  return Status::Success();
}

}  // namespace

Status ImportKeyJwk(const CryptoData& key_data,
                    const blink::WebCryptoAlgorithm& algorithm_or_null,
                    bool extractable,
                    blink::WebCryptoKeyUsageMask usage_mask,
                    blink::WebCryptoKey* key) {
  // TODO(padolph): Generalize this comment to include export, and move to top
  // of file.

  // TODO(padolph): Generalize this comment to include export, and move to top
  // of file.

  // The goal of this method is to extract key material and meta data from the
  // incoming JWK, combine them with the input parameters, and ultimately import
  // a Web Crypto Key.
  //
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
  // it according to rules defined below. A special case is that the method
  // input parameter 'algorithm' is also optional. If it is null, the JWK 'alg'
  // value (if present) is used as a fallback.
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
  //   | "deriveBits" | key derivation TODO(padolph): not currently supported |
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
  //   | "RS256"      | RSASSA using SHA-256 hash algorithm                   |
  //   | "RS384"      | RSASSA using SHA-384 hash algorithm                   |
  //   | "RS512"      | RSASSA using SHA-512 hash algorithm                   |
  //   +--------------+-------------------------------------------------------|
  //   | Key Management Algorithm                                             |
  //   +--------------+-------------------------------------------------------+
  //   | "RSA1_5"     | RSAES-PKCS1-V1_5 [RFC3447]                            |
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
  // carried in the JWK to be imported. Currently only two possibilities are
  // supported: a raw key or an RSA public key. RSA private keys are not
  // supported because typical applications seldom need to import a private key,
  // and the large number of JWK parameters required to describe one.
  // - kty == "oct" (symmetric or other raw key)
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
  //
  // Consistency and conflict resolution
  // The 'algorithm_or_null', 'extractable', and 'usage_mask' input parameters
  // may be different than the corresponding values inside the JWK. The Web
  // Crypto spec says that if a JWK value is present but is inconsistent with
  // the input value, it is an error and the operation must fail. If no
  // inconsistency is found, the input and JWK values are combined as follows:
  //
  // algorithm
  //   If an algorithm is provided by both the input parameter and the JWK,
  //   consistency between the two is based only on algorithm ID's (including an
  //   inner hash algorithm if present). In this case if the consistency
  //   check is passed, the input algorithm is used. If only one of either the
  //   input algorithm and JWK alg is provided, it is used as the final
  //   algorithm.
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

  if (!key_data.byte_length())
    return Status::ErrorImportEmptyKeyData();
  DCHECK(key);

  // Parse the incoming JWK JSON.
  base::StringPiece json_string(reinterpret_cast<const char*>(key_data.bytes()),
                                key_data.byte_length());
  scoped_ptr<base::Value> value(base::JSONReader::Read(json_string));
  // Note, bare pointer dict_value is ok since it points into scoped value.
  base::DictionaryValue* dict_value = NULL;
  if (!value.get() || !value->GetAsDictionary(&dict_value) || !dict_value)
    return Status::ErrorJwkNotDictionary();

  // JWK "kty". Exit early if this required JWK parameter is missing.
  std::string jwk_kty_value;
  Status status = GetJwkString(dict_value, "kty", &jwk_kty_value);
  if (status.IsError())
    return status;

  // JWK "ext" (optional) --> extractable parameter
  {
    bool jwk_ext_value = false;
    bool has_jwk_ext;
    status =
        GetOptionalJwkBool(dict_value, "ext", &jwk_ext_value, &has_jwk_ext);
    if (status.IsError())
      return status;
    if (has_jwk_ext && !jwk_ext_value && extractable)
      return Status::ErrorJwkExtInconsistent();
  }

  // JWK "alg" (optional) --> algorithm parameter
  // Note: input algorithm is also optional, so we have six cases to handle.
  // 1. JWK alg present but unrecognized: error
  // 2. JWK alg valid AND input algorithm isNull: use JWK value
  // 3. JWK alg valid AND input algorithm specified, but JWK value
  //      inconsistent with input: error
  // 4. JWK alg valid AND input algorithm specified, both consistent: use
  //      input value (because it has potentially more details)
  // 5. JWK alg missing AND input algorithm isNull: error
  // 6. JWK alg missing AND input algorithm specified: use input value
  blink::WebCryptoAlgorithm algorithm = blink::WebCryptoAlgorithm::createNull();
  const JwkAlgorithmInfo* algorithm_info = NULL;
  std::string jwk_alg_value;
  bool has_jwk_alg;
  status =
      GetOptionalJwkString(dict_value, "alg", &jwk_alg_value, &has_jwk_alg);
  if (status.IsError())
    return status;

  if (has_jwk_alg) {
    // JWK alg present

    // TODO(padolph): Validate alg vs kty. For example kty="RSA" implies alg can
    // only be from the RSA family.

    blink::WebCryptoAlgorithm jwk_algorithm =
        blink::WebCryptoAlgorithm::createNull();
    algorithm_info = jwk_alg_registry.Get().GetAlgorithmInfo(jwk_alg_value);
    if (!algorithm_info ||
        !algorithm_info->CreateImportAlgorithm(&jwk_algorithm))
      return Status::ErrorJwkUnrecognizedAlgorithm();  // case 1

    // JWK alg valid
    if (algorithm_or_null.isNull()) {
      // input algorithm not specified
      algorithm = jwk_algorithm;  // case 2
    } else {
      // input algorithm specified
      if (!ImportAlgorithmsConsistent(jwk_algorithm, algorithm_or_null))
        return Status::ErrorJwkAlgorithmInconsistent();  // case 3
      algorithm = algorithm_or_null;                     // case 4
    }
  } else {
    // JWK alg missing
    if (algorithm_or_null.isNull())
      return Status::ErrorJwkAlgorithmMissing();  // case 5
    algorithm = algorithm_or_null;                // case 6
  }
  DCHECK(!algorithm.isNull());

  // JWK "key_ops" (optional) --> usage_mask parameter
  base::ListValue* jwk_key_ops_value = NULL;
  bool has_jwk_key_ops;
  status = GetOptionalJwkList(
      dict_value, "key_ops", &jwk_key_ops_value, &has_jwk_key_ops);
  if (status.IsError())
    return status;
  blink::WebCryptoKeyUsageMask jwk_key_ops_mask = 0;
  if (has_jwk_key_ops) {
    status =
        GetWebCryptoUsagesFromJwkKeyOps(jwk_key_ops_value, &jwk_key_ops_mask);
    if (status.IsError())
      return status;
    // The input usage_mask must be a subset of jwk_key_ops_mask.
    if (!ContainsKeyUsages(jwk_key_ops_mask, usage_mask))
      return Status::ErrorJwkKeyopsInconsistent();
  }

  // JWK "use" (optional) --> usage_mask parameter
  std::string jwk_use_value;
  bool has_jwk_use;
  status =
      GetOptionalJwkString(dict_value, "use", &jwk_use_value, &has_jwk_use);
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
    if (!ContainsKeyUsages(jwk_use_mask, usage_mask))
      return Status::ErrorJwkUseInconsistent();
  }

  // If both 'key_ops' and 'use' are present, ensure they are consistent.
  if (has_jwk_key_ops && has_jwk_use &&
      !ContainsKeyUsages(jwk_use_mask, jwk_key_ops_mask))
    return Status::ErrorJwkUseAndKeyopsInconsistent();

  // JWK keying material --> ImportKeyInternal()
  if (jwk_kty_value == "oct") {
    std::string jwk_k_value;
    status = GetJwkBytes(dict_value, "k", &jwk_k_value);
    if (status.IsError())
      return status;

    // Some JWK alg ID's embed information about the key length in the alg ID
    // string. For example "A128CBC" implies the JWK carries 128 bits
    // of key material. For such keys validate that enough bytes were provided.
    // If this validation is not done, then it would be possible to select a
    // different algorithm by passing a different lengthed key, since that is
    // how WebCrypto interprets things.
    if (algorithm_info &&
        algorithm_info->IsInvalidKeyByteLength(jwk_k_value.size())) {
      return Status::ErrorJwkIncorrectKeyLength();
    }

    return ImportKey(blink::WebCryptoKeyFormatRaw,
                     CryptoData(jwk_k_value),
                     algorithm,
                     extractable,
                     usage_mask,
                     key);
  }
  if (jwk_kty_value == "RSA") {
    // An RSA public key must have an "n" (modulus) and an "e" (exponent) entry
    // in the JWK, while an RSA private key must have those, plus at least a "d"
    // (private exponent) entry.
    // See http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-18,
    // section 6.3.

    // RSA private key import is not currently supported, so fail here if a "d"
    // entry is found.
    // TODO(padolph): Support RSA private key import.
    if (dict_value->HasKey("d"))
      return Status::ErrorJwkRsaPrivateKeyUnsupported();

    std::string jwk_n_value;
    status = GetJwkBytes(dict_value, "n", &jwk_n_value);
    if (status.IsError())
      return status;
    std::string jwk_e_value;
    status = GetJwkBytes(dict_value, "e", &jwk_e_value);
    if (status.IsError())
      return status;

    return platform::ImportRsaPublicKey(algorithm,
                                        extractable,
                                        usage_mask,
                                        CryptoData(jwk_n_value),
                                        CryptoData(jwk_e_value),
                                        key);

  }

  return Status::ErrorJwkUnrecognizedKty();
}

Status ExportKeyJwk(const blink::WebCryptoKey& key,
                    blink::WebArrayBuffer* buffer) {
  base::DictionaryValue jwk_dict;
  Status status = Status::Error();
  blink::WebArrayBuffer exported_key;

  if (key.type() == blink::WebCryptoKeyTypeSecret) {
    status = ExportKey(blink::WebCryptoKeyFormatRaw, key, &exported_key);
    if (status.IsError())
      return status;
    WriteSecretKey(exported_key, &jwk_dict);
  } else {
    // TODO(padolph): Handle asymmetric keys, at least the public key.
    return Status::ErrorUnsupported();
  }

  WriteKeyOps(key.usages(), &jwk_dict);
  WriteExt(key.extractable(), &jwk_dict);
  status = WriteAlg(key.algorithm(), exported_key.byteLength(), &jwk_dict);
  if (status.IsError())
    return status;

  std::string json;
  base::JSONWriter::Write(&jwk_dict, &json);
  *buffer = CreateArrayBuffer(reinterpret_cast<const uint8*>(json.data()),
                              json.size());
  return Status::Success();
}

}  // namespace webcrypto

}  // namespace content
