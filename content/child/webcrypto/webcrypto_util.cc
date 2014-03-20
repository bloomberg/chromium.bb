// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/webcrypto_util.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "content/child/webcrypto/status.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

const uint8* Uint8VectorStart(const std::vector<uint8>& data) {
  if (data.empty())
    return NULL;
  return &data[0];
}

void ShrinkBuffer(blink::WebArrayBuffer* buffer, unsigned int new_size) {
  DCHECK_LE(new_size, buffer->byteLength());

  if (new_size == buffer->byteLength())
    return;

  blink::WebArrayBuffer new_buffer = blink::WebArrayBuffer::create(new_size, 1);
  DCHECK(!new_buffer.isNull());
  memcpy(new_buffer.data(), buffer->data(), new_size);
  *buffer = new_buffer;
}

blink::WebArrayBuffer CreateArrayBuffer(const uint8* data,
                                        unsigned int data_size) {
  blink::WebArrayBuffer buffer = blink::WebArrayBuffer::create(data_size, 1);
  DCHECK(!buffer.isNull());
  if (data_size)  // data_size == 0 might mean the data pointer is invalid
    memcpy(buffer.data(), data, data_size);
  return buffer;
}

// This function decodes unpadded 'base64url' encoded data, as described in
// RFC4648 (http://www.ietf.org/rfc/rfc4648.txt) Section 5. To do this, first
// change the incoming data to 'base64' encoding by applying the appropriate
// transformation including adding padding if required, and then call a base64
// decoder.
bool Base64DecodeUrlSafe(const std::string& input, std::string* output) {
  std::string base64EncodedText(input);
  std::replace(base64EncodedText.begin(), base64EncodedText.end(), '-', '+');
  std::replace(base64EncodedText.begin(), base64EncodedText.end(), '_', '/');
  base64EncodedText.append((4 - base64EncodedText.size() % 4) % 4, '=');
  return base::Base64Decode(base64EncodedText, output);
}

// Returns an unpadded 'base64url' encoding of the input data, using the
// inverse of the process above.
std::string Base64EncodeUrlSafe(const base::StringPiece& input) {
  std::string output;
  base::Base64Encode(input, &output);
  std::replace(output.begin(), output.end(), '+', '-');
  std::replace(output.begin(), output.end(), '/', '_');
  output.erase(std::remove(output.begin(), output.end(), '='), output.end());
  return output;
}

struct JwkToWebCryptoUsage {
  const char* const jwk_key_op;
  const blink::WebCryptoKeyUsage webcrypto_usage;
};

const JwkToWebCryptoUsage kJwkWebCryptoUsageMap[] = {
    {"encrypt", blink::WebCryptoKeyUsageEncrypt},
    {"decrypt", blink::WebCryptoKeyUsageDecrypt},
    {"deriveKey", blink::WebCryptoKeyUsageDeriveKey},
    // TODO(padolph): Add 'deriveBits' once supported by Blink.
    {"sign", blink::WebCryptoKeyUsageSign},
    {"unwrapKey", blink::WebCryptoKeyUsageUnwrapKey},
    {"verify", blink::WebCryptoKeyUsageVerify},
    {"wrapKey", blink::WebCryptoKeyUsageWrapKey}};

// Modifies the input usage_mask by according to the key_op value.
bool JwkKeyOpToWebCryptoUsage(const std::string& key_op,
                              blink::WebCryptoKeyUsageMask* usage_mask) {
  for (size_t i = 0; i < arraysize(kJwkWebCryptoUsageMap); ++i) {
    if (kJwkWebCryptoUsageMap[i].jwk_key_op == key_op) {
      *usage_mask |= kJwkWebCryptoUsageMap[i].webcrypto_usage;
      return true;
    }
  }
  return false;
}

// Composes a Web Crypto usage mask from an array of JWK key_ops values.
Status GetWebCryptoUsagesFromJwkKeyOps(
    const base::ListValue* jwk_key_ops_value,
    blink::WebCryptoKeyUsageMask* usage_mask) {
  *usage_mask = 0;
  for (size_t i = 0; i < jwk_key_ops_value->GetSize(); ++i) {
    std::string key_op;
    if (!jwk_key_ops_value->GetString(i, &key_op)) {
      return Status::ErrorJwkPropertyWrongType(
          base::StringPrintf("key_ops[%d]", static_cast<int>(i)), "string");
    }
    if (!JwkKeyOpToWebCryptoUsage(key_op, usage_mask))
      return Status::ErrorJwkUnrecognizedKeyop();
  }
  return Status::Success();
}

// Composes a JWK key_ops List from a Web Crypto usage mask.
// Note: Caller must assume ownership of returned instance.
base::ListValue* CreateJwkKeyOpsFromWebCryptoUsages(
    blink::WebCryptoKeyUsageMask usage_mask) {
  base::ListValue* jwk_key_ops = new base::ListValue();
  for (size_t i = 0; i < arraysize(kJwkWebCryptoUsageMap); ++i) {
    if (usage_mask & kJwkWebCryptoUsageMap[i].webcrypto_usage)
      jwk_key_ops->AppendString(kJwkWebCryptoUsageMap[i].jwk_key_op);
  }
  return jwk_key_ops;
}

bool IsHashAlgorithm(blink::WebCryptoAlgorithmId alg_id) {
  return alg_id == blink::WebCryptoAlgorithmIdSha1 ||
         alg_id == blink::WebCryptoAlgorithmIdSha256 ||
         alg_id == blink::WebCryptoAlgorithmIdSha384 ||
         alg_id == blink::WebCryptoAlgorithmIdSha512;
}

blink::WebCryptoAlgorithm GetInnerHashAlgorithm(
    const blink::WebCryptoAlgorithm& algorithm) {
  DCHECK(!algorithm.isNull());
  switch (algorithm.paramsType()) {
    case blink::WebCryptoAlgorithmParamsTypeHmacImportParams:
      return algorithm.hmacImportParams()->hash();
    case blink::WebCryptoAlgorithmParamsTypeHmacKeyGenParams:
      return algorithm.hmacKeyGenParams()->hash();
    case blink::WebCryptoAlgorithmParamsTypeRsaHashedImportParams:
      return algorithm.rsaHashedImportParams()->hash();
    case blink::WebCryptoAlgorithmParamsTypeRsaHashedKeyGenParams:
      return algorithm.rsaHashedKeyGenParams()->hash();
    default:
      return blink::WebCryptoAlgorithm::createNull();
  }
}

blink::WebCryptoAlgorithm CreateAlgorithm(blink::WebCryptoAlgorithmId id) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(id, NULL);
}

blink::WebCryptoAlgorithm CreateHmacImportAlgorithm(
    blink::WebCryptoAlgorithmId hash_id) {
  DCHECK(IsHashAlgorithm(hash_id));
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdHmac,
      new blink::WebCryptoHmacImportParams(CreateAlgorithm(hash_id)));
}

blink::WebCryptoAlgorithm CreateRsaHashedImportAlgorithm(
    blink::WebCryptoAlgorithmId id,
    blink::WebCryptoAlgorithmId hash_id) {
  DCHECK(IsHashAlgorithm(hash_id));
  DCHECK(id == blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 ||
         id == blink::WebCryptoAlgorithmIdRsaOaep);
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      id, new blink::WebCryptoRsaHashedImportParams(CreateAlgorithm(hash_id)));
}

blink::WebCryptoAlgorithm CreateRsaSsaImportAlgorithm(
    blink::WebCryptoAlgorithmId hash_id) {
  return CreateRsaHashedImportAlgorithm(
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5, hash_id);
}

blink::WebCryptoAlgorithm CreateRsaOaepImportAlgorithm(
    blink::WebCryptoAlgorithmId hash_id) {
  return CreateRsaHashedImportAlgorithm(blink::WebCryptoAlgorithmIdRsaOaep,
                                        hash_id);
}

unsigned int ShaBlockSizeBytes(blink::WebCryptoAlgorithmId hash_id) {
  switch (hash_id) {
    case blink::WebCryptoAlgorithmIdSha1:
    case blink::WebCryptoAlgorithmIdSha256:
      return 64;
    case blink::WebCryptoAlgorithmIdSha384:
    case blink::WebCryptoAlgorithmIdSha512:
      return 128;
    default:
      NOTREACHED();
      return 0;
  }
}

bool CreateSecretKeyAlgorithm(const blink::WebCryptoAlgorithm& algorithm,
                              unsigned int keylen_bytes,
                              blink::WebCryptoKeyAlgorithm* key_algorithm) {
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdHmac: {
      blink::WebCryptoAlgorithm hash = GetInnerHashAlgorithm(algorithm);
      if (hash.isNull())
        return false;
#if defined(WEBCRYPTO_HMAC_KEY_HAS_LENGTH)
      if (keylen_bytes > UINT_MAX / 8)
        return false;
#endif
      *key_algorithm = blink::WebCryptoKeyAlgorithm::adoptParamsAndCreate(
          algorithm.id(),
#if defined(WEBCRYPTO_HMAC_KEY_HAS_LENGTH)
          new blink::WebCryptoHmacKeyAlgorithmParams(hash, keylen_bytes * 8));
#else
          new blink::WebCryptoHmacKeyAlgorithmParams(hash));
#endif
      return true;
    }
    case blink::WebCryptoAlgorithmIdAesKw:
    case blink::WebCryptoAlgorithmIdAesCbc:
    case blink::WebCryptoAlgorithmIdAesCtr:
    case blink::WebCryptoAlgorithmIdAesGcm:
      *key_algorithm = blink::WebCryptoKeyAlgorithm::adoptParamsAndCreate(
          algorithm.id(),
          new blink::WebCryptoAesKeyAlgorithmParams(keylen_bytes * 8));
      return true;
    default:
      return false;
  }
}

}  // namespace webcrypto

}  // namespace content
