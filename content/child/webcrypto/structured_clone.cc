// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/webcrypto/structured_clone.h"

#include "base/logging.h"
#include "content/child/webcrypto/algorithm_dispatch.h"
#include "content/child/webcrypto/platform_crypto.h"
#include "content/child/webcrypto/status.h"
#include "content/child/webcrypto/webcrypto_util.h"
#include "third_party/WebKit/public/platform/WebCryptoKeyAlgorithm.h"

namespace content {

namespace webcrypto {

namespace {

// Returns the key format to use for structured cloning.
blink::WebCryptoKeyFormat GetCloneFormatForKeyType(
    blink::WebCryptoKeyType type) {
  switch (type) {
    case blink::WebCryptoKeyTypeSecret:
      return blink::WebCryptoKeyFormatRaw;
    case blink::WebCryptoKeyTypePublic:
      return blink::WebCryptoKeyFormatSpki;
    case blink::WebCryptoKeyTypePrivate:
      return blink::WebCryptoKeyFormatPkcs8;
  }

  NOTREACHED();
  return blink::WebCryptoKeyFormatRaw;
}

// Converts a KeyAlgorithm into an equivalent Algorithm for import.
blink::WebCryptoAlgorithm KeyAlgorithmToImportAlgorithm(
    const blink::WebCryptoKeyAlgorithm& algorithm) {
  switch (algorithm.paramsType()) {
    case blink::WebCryptoKeyAlgorithmParamsTypeAes:
      return CreateAlgorithm(algorithm.id());
    case blink::WebCryptoKeyAlgorithmParamsTypeHmac:
      return CreateHmacImportAlgorithm(algorithm.hmacParams()->hash().id());
    case blink::WebCryptoKeyAlgorithmParamsTypeRsaHashed:
      return CreateRsaHashedImportAlgorithm(
          algorithm.id(), algorithm.rsaHashedParams()->hash().id());
    case blink::WebCryptoKeyAlgorithmParamsTypeNone:
      break;
    default:
      break;
  }
  return blink::WebCryptoAlgorithm::createNull();
}

// There is some duplicated information in the serialized format used by
// structured clone (since the KeyAlgorithm is serialized separately from the
// key data). Use this extra information to further validate what was
// deserialized from the key data.
//
// A failure here implies either a bug in the code, or that the serialized data
// was corrupted.
bool ValidateDeserializedKey(const blink::WebCryptoKey& key,
                             const blink::WebCryptoKeyAlgorithm& algorithm,
                             blink::WebCryptoKeyType type) {
  if (algorithm.id() != key.algorithm().id())
    return false;

  if (key.type() != type)
    return false;

  switch (algorithm.paramsType()) {
    case blink::WebCryptoKeyAlgorithmParamsTypeAes:
      if (algorithm.aesParams()->lengthBits() !=
          key.algorithm().aesParams()->lengthBits())
        return false;
      break;
    case blink::WebCryptoKeyAlgorithmParamsTypeRsaHashed:
      if (algorithm.rsaHashedParams()->modulusLengthBits() !=
          key.algorithm().rsaHashedParams()->modulusLengthBits())
        return false;
      if (algorithm.rsaHashedParams()->publicExponent().size() !=
          key.algorithm().rsaHashedParams()->publicExponent().size())
        return false;
      if (memcmp(algorithm.rsaHashedParams()->publicExponent().data(),
                 key.algorithm().rsaHashedParams()->publicExponent().data(),
                 key.algorithm().rsaHashedParams()->publicExponent().size()) !=
          0)
        return false;
      break;
    case blink::WebCryptoKeyAlgorithmParamsTypeNone:
    case blink::WebCryptoKeyAlgorithmParamsTypeHmac:
      break;
    default:
      return false;
  }

  return true;
}

}  // namespace

// Note that this function is called from the target Blink thread.
bool SerializeKeyForClone(const blink::WebCryptoKey& key,
                          blink::WebVector<uint8_t>* key_data) {
  return PlatformSerializeKeyForClone(key, key_data);
}

// Note that this function is called from the target Blink thread.
bool DeserializeKeyForClone(const blink::WebCryptoKeyAlgorithm& algorithm,
                            blink::WebCryptoKeyType type,
                            bool extractable,
                            blink::WebCryptoKeyUsageMask usage_mask,
                            const CryptoData& key_data,
                            blink::WebCryptoKey* key) {
  // TODO(eroman): This should not call into the platform crypto layer.
  // Otherwise it runs the risk of stalling while the NSS/OpenSSL global locks
  // are held.
  //
  // An alternate approach is to defer the key import until the key is used.
  // However this means that any deserialization errors would have to be
  // surfaced as WebCrypto errors, leading to slightly different behaviors. For
  // instance you could clone a key which fails to be deserialized.
  Status status = ImportKey(GetCloneFormatForKeyType(type),
                            key_data,
                            KeyAlgorithmToImportAlgorithm(algorithm),
                            extractable,
                            usage_mask,
                            key);
  if (status.IsError())
    return false;
  return ValidateDeserializedKey(*key, algorithm, type);
}

}  // namespace webcrypto

}  // namespace content
