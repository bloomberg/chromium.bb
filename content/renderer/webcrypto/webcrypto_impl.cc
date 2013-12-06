// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto/webcrypto_impl.h"

#include <algorithm>
#include <functional>
#include <map>
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "content/renderer/webcrypto/webcrypto_util.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"

namespace content {

namespace {

bool IsAlgorithmAsymmetric(const blink::WebCryptoAlgorithm& algorithm) {
  // TODO(padolph): include all other asymmetric algorithms once they are
  // defined, e.g. EC and DH.
  return (algorithm.id() == blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5 ||
          algorithm.id() == blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 ||
          algorithm.id() == blink::WebCryptoAlgorithmIdRsaOaep);
}

// Binds a specific key length value to a compatible factory function.
typedef blink::WebCryptoAlgorithm (*AlgFactoryFuncWithOneShortArg)(
    unsigned short);
template <AlgFactoryFuncWithOneShortArg func, unsigned short key_length>
blink::WebCryptoAlgorithm BindAlgFactoryWithKeyLen() {
  return func(key_length);
}

// Binds a WebCryptoAlgorithmId value to a compatible factory function.
typedef blink::WebCryptoAlgorithm (*AlgFactoryFuncWithWebCryptoAlgIdArg)(
    blink::WebCryptoAlgorithmId);
template <AlgFactoryFuncWithWebCryptoAlgIdArg func,
          blink::WebCryptoAlgorithmId algorithm_id>
blink::WebCryptoAlgorithm BindAlgFactoryAlgorithmId() {
  return func(algorithm_id);
}

// Defines a map between a JWK 'alg' string ID and a corresponding Web Crypto
// factory function.
typedef blink::WebCryptoAlgorithm (*AlgFactoryFuncNoArgs)();
typedef std::map<std::string, AlgFactoryFuncNoArgs> JwkAlgFactoryMap;

class JwkAlgorithmFactoryMap {
 public:
  JwkAlgorithmFactoryMap() {
    map_["HS256"] =
        &BindAlgFactoryWithKeyLen<webcrypto::CreateHmacAlgorithmByHashOutputLen,
                                  256>;
    map_["HS384"] =
        &BindAlgFactoryWithKeyLen<webcrypto::CreateHmacAlgorithmByHashOutputLen,
                                  384>;
    map_["HS512"] =
        &BindAlgFactoryWithKeyLen<webcrypto::CreateHmacAlgorithmByHashOutputLen,
                                  512>;
    map_["RS256"] =
        &BindAlgFactoryAlgorithmId<webcrypto::CreateRsaSsaAlgorithm,
                                   blink::WebCryptoAlgorithmIdSha256>;
    map_["RS384"] =
        &BindAlgFactoryAlgorithmId<webcrypto::CreateRsaSsaAlgorithm,
                                   blink::WebCryptoAlgorithmIdSha384>;
    map_["RS512"] =
        &BindAlgFactoryAlgorithmId<webcrypto::CreateRsaSsaAlgorithm,
                                   blink::WebCryptoAlgorithmIdSha512>;
    map_["RSA1_5"] =
        &BindAlgFactoryAlgorithmId<webcrypto::CreateAlgorithm,
                                   blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5>;
    map_["RSA-OAEP"] =
        &BindAlgFactoryAlgorithmId<webcrypto::CreateRsaOaepAlgorithm,
                                   blink::WebCryptoAlgorithmIdSha1>;
    // TODO(padolph): The Web Crypto spec does not enumerate AES-KW 128 yet
    map_["A128KW"] = &blink::WebCryptoAlgorithm::createNull;
    // TODO(padolph): The Web Crypto spec does not enumerate AES-KW 256 yet
    map_["A256KW"] = &blink::WebCryptoAlgorithm::createNull;
    map_["A128GCM"] =
        &BindAlgFactoryAlgorithmId<webcrypto::CreateAlgorithm,
                                   blink::WebCryptoAlgorithmIdAesGcm>;
    map_["A256GCM"] =
        &BindAlgFactoryAlgorithmId<webcrypto::CreateAlgorithm,
                                   blink::WebCryptoAlgorithmIdAesGcm>;
    map_["A128CBC"] =
        &BindAlgFactoryAlgorithmId<webcrypto::CreateAlgorithm,
                                   blink::WebCryptoAlgorithmIdAesCbc>;
    map_["A192CBC"] =
        &BindAlgFactoryAlgorithmId<webcrypto::CreateAlgorithm,
                                   blink::WebCryptoAlgorithmIdAesCbc>;
    map_["A256CBC"] =
        &BindAlgFactoryAlgorithmId<webcrypto::CreateAlgorithm,
                                   blink::WebCryptoAlgorithmIdAesCbc>;
  }

  blink::WebCryptoAlgorithm CreateAlgorithmFromName(const std::string& alg_id)
      const {
    const JwkAlgFactoryMap::const_iterator pos = map_.find(alg_id);
    if (pos == map_.end())
      return blink::WebCryptoAlgorithm::createNull();
    return pos->second();
  }

 private:
  JwkAlgFactoryMap map_;
};

base::LazyInstance<JwkAlgorithmFactoryMap> jwk_alg_factory =
    LAZY_INSTANCE_INITIALIZER;

bool WebCryptoAlgorithmsConsistent(const blink::WebCryptoAlgorithm& alg1,
                                   const blink::WebCryptoAlgorithm& alg2) {
  DCHECK(!alg1.isNull());
  DCHECK(!alg2.isNull());
  if (alg1.id() != alg2.id())
    return false;
  switch (alg1.id()) {
    case blink::WebCryptoAlgorithmIdHmac:
    case blink::WebCryptoAlgorithmIdRsaOaep:
    case blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
      if (WebCryptoAlgorithmsConsistent(
              webcrypto::GetInnerHashAlgorithm(alg1),
              webcrypto::GetInnerHashAlgorithm(alg2))) {
        return true;
      }
      break;
    case blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5:
    case blink::WebCryptoAlgorithmIdSha1:
    case blink::WebCryptoAlgorithmIdSha224:
    case blink::WebCryptoAlgorithmIdSha256:
    case blink::WebCryptoAlgorithmIdSha384:
    case blink::WebCryptoAlgorithmIdSha512:
    case blink::WebCryptoAlgorithmIdAesCbc:
    case blink::WebCryptoAlgorithmIdAesGcm:
    case blink::WebCryptoAlgorithmIdAesCtr:
      return true;
    default:
      NOTREACHED();  // Not a supported algorithm.
      break;
  }
  return false;
}

bool GetDecodedUrl64ValueByKey(
    const base::DictionaryValue& dict,
    const std::string& key,
    std::string* decoded) {
  std::string value_url64;
  if (!dict.GetString(key, &value_url64) ||
      !webcrypto::Base64DecodeUrlSafe(value_url64, decoded) ||
      !decoded->size()) {
    return false;
  }
  return true;
}

}  // namespace

WebCryptoImpl::WebCryptoImpl() {
  Init();
}

void WebCryptoImpl::encrypt(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
  blink::WebArrayBuffer buffer;
  if (!EncryptInternal(algorithm, key, data, data_size, &buffer)) {
    result.completeWithError();
  } else {
    result.completeWithBuffer(buffer);
  }
}

void WebCryptoImpl::decrypt(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
  blink::WebArrayBuffer buffer;
  if (!DecryptInternal(algorithm, key, data, data_size, &buffer)) {
    result.completeWithError();
  } else {
    result.completeWithBuffer(buffer);
  }
}

void WebCryptoImpl::digest(
    const blink::WebCryptoAlgorithm& algorithm,
    const unsigned char* data,
    unsigned data_size,
    blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
  blink::WebArrayBuffer buffer;
  if (!DigestInternal(algorithm, data, data_size, &buffer)) {
    result.completeWithError();
  } else {
    result.completeWithBuffer(buffer);
  }
}

void WebCryptoImpl::generateKey(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
  if (IsAlgorithmAsymmetric(algorithm)) {
    blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
    blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
    if (!GenerateKeyPairInternal(
             algorithm, extractable, usage_mask, &public_key, &private_key)) {
      result.completeWithError();
    } else {
      DCHECK(public_key.handle());
      DCHECK(private_key.handle());
      DCHECK_EQ(algorithm.id(), public_key.algorithm().id());
      DCHECK_EQ(algorithm.id(), private_key.algorithm().id());
      DCHECK_EQ(true, public_key.extractable());
      DCHECK_EQ(extractable, private_key.extractable());
      DCHECK_EQ(usage_mask, public_key.usages());
      DCHECK_EQ(usage_mask, private_key.usages());
      result.completeWithKeyPair(public_key, private_key);
    }
  } else {
    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
    if (!GenerateKeyInternal(algorithm, extractable, usage_mask, &key)) {
      result.completeWithError();
    } else {
      DCHECK(key.handle());
      DCHECK_EQ(algorithm.id(), key.algorithm().id());
      DCHECK_EQ(extractable, key.extractable());
      DCHECK_EQ(usage_mask, key.usages());
      result.completeWithKey(key);
    }
  }
}

void WebCryptoImpl::importKey(
    blink::WebCryptoKeyFormat format,
    const unsigned char* key_data,
    unsigned key_data_size,
    const blink::WebCryptoAlgorithm& algorithm_or_null,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoResult result) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  if (format == blink::WebCryptoKeyFormatJwk) {
    if (!ImportKeyJwk(key_data,
                      key_data_size,
                      algorithm_or_null,
                      extractable,
                      usage_mask,
                      &key)) {
      result.completeWithError();
      return;
    }
  } else {
    if (!ImportKeyInternal(format,
                           key_data,
                           key_data_size,
                           algorithm_or_null,
                           extractable,
                           usage_mask,
                           &key)) {
      result.completeWithError();
      return;
    }
  }
  DCHECK(key.handle());
  DCHECK(!key.algorithm().isNull());
  DCHECK_EQ(extractable, key.extractable());
  result.completeWithKey(key);
}

void WebCryptoImpl::exportKey(
    blink::WebCryptoKeyFormat format,
    const blink::WebCryptoKey& key,
    blink::WebCryptoResult result) {
  blink::WebArrayBuffer buffer;
  if (!ExportKeyInternal(format, key, &buffer)) {
    result.completeWithError();
    return;
  }
  result.completeWithBuffer(buffer);
}

void WebCryptoImpl::sign(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
  blink::WebArrayBuffer buffer;
  if (!SignInternal(algorithm, key, data, data_size, &buffer)) {
    result.completeWithError();
  } else {
    result.completeWithBuffer(buffer);
  }
}

void WebCryptoImpl::verifySignature(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    const unsigned char* signature,
    unsigned signature_size,
    const unsigned char* data,
    unsigned data_size,
    blink::WebCryptoResult result) {
  DCHECK(!algorithm.isNull());
  bool signature_match = false;
  if (!VerifySignatureInternal(algorithm,
                               key,
                               signature,
                               signature_size,
                               data,
                               data_size,
                               &signature_match)) {
    result.completeWithError();
  } else {
    result.completeWithBoolean(signature_match);
  }
}

bool WebCryptoImpl::ImportKeyJwk(
    const unsigned char* key_data,
    unsigned key_data_size,
    const blink::WebCryptoAlgorithm& algorithm_or_null,
    bool extractable,
    blink::WebCryptoKeyUsageMask usage_mask,
    blink::WebCryptoKey* key) {

  // The goal of this method is to extract key material and meta data from the
  // incoming JWK, combine them with the input parameters, and ultimately import
  // a Web Crypto Key.
  //
  // JSON Web Key Format (JWK)
  // http://tools.ietf.org/html/draft-ietf-jose-json-web-key-16
  // TODO(padolph): Not all possible values are handled by this code right now
  //
  // A JWK is a simple JSON dictionary with the following entries
  // - "kty" (Key Type) Parameter, REQUIRED
  // - <kty-specific parameters, see below>, REQUIRED
  // - "use" (Key Use) Parameter, OPTIONAL
  // - "alg" (Algorithm) Parameter, OPTIONAL
  // - "extractable" (Key Exportability), OPTIONAL [NOTE: not yet part of JOSE,
  //   see https://www.w3.org/Bugs/Public/show_bug.cgi?id=23796]
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
  // Web Crypto Key extractable     <-- JWK extractable + input extractable
  // Web Crypto Key algorithm       <-- JWK alg + input algorithm
  // Web Crypto Key keyUsage        <-- JWK use + input usage_mask
  // Web Crypto Key keying material <-- kty-specific parameters
  //
  // Values for each JWK entry are case-sensitive and defined in
  // http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-16.
  // Note that not all values specified by JOSE are handled by this code. Only
  // handled values are listed.
  // - kty (Key Type)
  //   +-------+--------------------------------------------------------------+
  //   | "RSA" | RSA [RFC3447]                                                |
  //   | "oct" | Octet sequence (used to represent symmetric keys)            |
  //   +-------+--------------------------------------------------------------+
  // - use (Key Use)
  //   +-------+--------------------------------------------------------------+
  //   | "enc" | encrypt and decrypt operations                               |
  //   | "sig" | sign and verify (MAC) operations                             |
  //   | "wrap"| key wrap and unwrap [not yet part of JOSE]                   |
  //   +-------+--------------------------------------------------------------+
  // - extractable (Key Exportability)
  //   +-------+--------------------------------------------------------------+
  //   | true  | Key may be exported from the trusted environment             |
  //   | false | Key cannot exit the trusted environment                      |
  //   +-------+--------------------------------------------------------------+
  // - alg (Algorithm)
  //   See http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-16
  //   +--------------+-------------------------------------------------------+
  //   | Digital Signature or MAC Algorithm                                   |
  //   +--------------+-------------------------------------------------------+
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
  //   | "A256KW"     | AES Key Wrap Algorithm using 256 bit keys             |
  //   | "A128GCM"    | AES in Galois/Counter Mode (GCM) [NIST.800-38D] using |
  //   |              | 128 bit keys                                          |
  //   | "A256GCM"    | AES GCM using 256 bit keys                            |
  //   | "A128CBC"    | AES in Cipher Block Chaining Mode (CBC) with PKCS #5  |
  //   |              | padding [NIST.800-38A] [not yet part of JOSE, see     |
  //   |              | https://www.w3.org/Bugs/Public/show_bug.cgi?id=23796  |
  //   | "A192CBC"    | AES CBC using 192 bit keys [not yet part of JOSE]     |
  //   | "A256CBC"    | AES CBC using 256 bit keys [not yet part of JOSE]     |
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
  //   If the JWK extractable is true but the input parameter is false, make the
  //   Web Crypto Key non-extractable. Conversely, if the JWK extractable is
  //   false but the input parameter is true, it is an inconsistency. If both
  //   are true or both are false, use that value.
  //
  // usage_mask
  //   The input usage_mask must be a strict subset of the interpreted JWK use
  //   value, else it is judged inconsistent. In all cases the input usage_mask
  //   is used as the final usage_mask.
  //

  if (!key_data_size)
    return false;
  DCHECK(key);

  // Parse the incoming JWK JSON.
  base::StringPiece json_string(reinterpret_cast<const char*>(key_data),
                                key_data_size);
  scoped_ptr<base::Value> value(base::JSONReader::Read(json_string));
  // Note, bare pointer dict_value is ok since it points into scoped value.
  base::DictionaryValue* dict_value = NULL;
  if (!value.get() || !value->GetAsDictionary(&dict_value) || !dict_value)
    return false;

  // JWK "kty". Exit early if this required JWK parameter is missing.
  std::string jwk_kty_value;
  if (!dict_value->GetString("kty", &jwk_kty_value))
    return false;

  // JWK "extractable" (optional) --> extractable parameter
  {
    bool jwk_extractable_value;
    if (dict_value->GetBoolean("extractable", &jwk_extractable_value)) {
      if (!jwk_extractable_value && extractable)
        return false;
      extractable = extractable && jwk_extractable_value;
    }
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
  std::string jwk_alg_value;
  if (dict_value->GetString("alg", &jwk_alg_value)) {
    // JWK alg present

    // TODO(padolph): Validate alg vs kty. For example kty="RSA" implies alg can
    // only be from the RSA family.

    const blink::WebCryptoAlgorithm jwk_algorithm =
        jwk_alg_factory.Get().CreateAlgorithmFromName(jwk_alg_value);
    if (jwk_algorithm.isNull()) {
      // JWK alg unrecognized
      return false;  // case 1
    }
    // JWK alg valid
    if (algorithm_or_null.isNull()) {
      // input algorithm not specified
      algorithm = jwk_algorithm;  // case 2
    } else {
      // input algorithm specified
      if (!WebCryptoAlgorithmsConsistent(jwk_algorithm, algorithm_or_null))
        return false;  // case 3
      algorithm = algorithm_or_null;  // case 4
    }
  } else {
    // JWK alg missing
    if (algorithm_or_null.isNull())
      return false;  // case 5
    algorithm = algorithm_or_null;  // case 6
  }
  DCHECK(!algorithm.isNull());

  // JWK "use" (optional) --> usage_mask parameter
  std::string jwk_use_value;
  if (dict_value->GetString("use", &jwk_use_value)) {
    blink::WebCryptoKeyUsageMask jwk_usage_mask = 0;
    if (jwk_use_value == "enc") {
      jwk_usage_mask =
          blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt;
    } else if (jwk_use_value == "sig") {
      jwk_usage_mask =
          blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify;
    } else if (jwk_use_value == "wrap") {
      jwk_usage_mask =
          blink::WebCryptoKeyUsageWrapKey | blink::WebCryptoKeyUsageUnwrapKey;
    } else {
      return false;
    }
    if ((jwk_usage_mask & usage_mask) != usage_mask) {
      // A usage_mask must be a subset of jwk_usage_mask.
      return false;
    }
  }

  // JWK keying material --> ImportKeyInternal()
  if (jwk_kty_value == "oct") {

    std::string jwk_k_value;
    if (!GetDecodedUrl64ValueByKey(*dict_value, "k", &jwk_k_value))
      return false;

    // TODO(padolph): Some JWK alg ID's embed information about the key length
    // in the alg ID string. For example "A128" implies the JWK carries 128 bits
    // of key material, and "HS512" implies the JWK carries _at least_ 512 bits
    // of key material. For such keys validate the actual key length against the
    // value in the ID.

    return ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                             reinterpret_cast<const uint8*>(jwk_k_value.data()),
                             jwk_k_value.size(),
                             algorithm,
                             extractable,
                             usage_mask,
                             key);
  } else if (jwk_kty_value == "RSA") {

    // An RSA public key must have an "n" (modulus) and an "e" (exponent) entry
    // in the JWK, while an RSA private key must have those, plus at least a "d"
    // (private exponent) entry.
    // See http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-18,
    // section 6.3.

    // RSA private key import is not currently supported, so fail here if a "d"
    // entry is found.
    // TODO(padolph): Support RSA private key import.
    if (dict_value->HasKey("d"))
      return false;

    std::string jwk_n_value;
    if (!GetDecodedUrl64ValueByKey(*dict_value, "n", &jwk_n_value))
      return false;
    std::string jwk_e_value;
    if (!GetDecodedUrl64ValueByKey(*dict_value, "e", &jwk_e_value))
      return false;

    return ImportRsaPublicKeyInternal(
        reinterpret_cast<const uint8*>(jwk_n_value.data()),
        jwk_n_value.size(),
        reinterpret_cast<const uint8*>(jwk_e_value.data()),
        jwk_e_value.size(),
        algorithm,
        extractable,
        usage_mask,
        key);

  } else {
    return false;
  }

  return true;
}

}  // namespace content
