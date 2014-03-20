// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_PLATFORM_CRYPTO_H_
#define CONTENT_CHILD_WEBCRYPTO_PLATFORM_CRYPTO_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCrypto.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

namespace content {

enum EncryptOrDecrypt { ENCRYPT, DECRYPT };

namespace webcrypto {

class CryptoData;
class Status;

// Functions in the webcrypto::platform namespace are intended to be those
// which are OpenSSL/NSS specific.
//
// The general purpose code which applies to both OpenSSL and NSS
// implementations of webcrypto should live in the outter webcrypto namespace,
// and the crypto library specific bits in the "platform" namespace.
namespace platform {

class SymKey;
class PublicKey;
class PrivateKey;

// Base key class for all platform keys, used to safely cast between types.
class Key : public blink::WebCryptoKeyHandle {
 public:
  virtual SymKey* AsSymKey() = 0;
  virtual PublicKey* AsPublicKey() = 0;
  virtual PrivateKey* AsPrivateKey() = 0;
};

// Do any one-time initialization. Note that this can be called MULTIPLE times
// (once per instantiation of WebCryptoImpl).
void Init();

// Preconditions:
//  * |key| is a non-null AES-CBC key.
//  * |iv| is exactly 16 bytes long
Status EncryptDecryptAesCbc(EncryptOrDecrypt mode,
                            SymKey* key,
                            const CryptoData& data,
                            const CryptoData& iv,
                            blink::WebArrayBuffer* buffer);

// Preconditions:
//  * |key| is a non-null AES-GCM key.
//  * |tag_length_bits| is one of {32, 64, 96, 104, 112, 120, 128}
Status EncryptDecryptAesGcm(EncryptOrDecrypt mode,
                            SymKey* key,
                            const CryptoData& data,
                            const CryptoData& iv,
                            const CryptoData& additional_data,
                            unsigned int tag_length_bits,
                            blink::WebArrayBuffer* buffer);

// Preconditions:
//  * |key| is non-null.
//  * |data| is not empty.
Status EncryptRsaEsPkcs1v1_5(PublicKey* key,
                             const CryptoData& data,
                             blink::WebArrayBuffer* buffer);

// Preconditions:
//  * |key| is non-null.
Status DecryptRsaEsPkcs1v1_5(PrivateKey* key,
                             const CryptoData& data,
                             blink::WebArrayBuffer* buffer);

// Preconditions:
//  * |key| is a non-null HMAC key.
//  * |hash| is a digest algorithm.
Status SignHmac(SymKey* key,
                const blink::WebCryptoAlgorithm& hash,
                const CryptoData& data,
                blink::WebArrayBuffer* buffer);

// Preconditions:
//  * |algorithm| is a SHA function.
Status DigestSha(blink::WebCryptoAlgorithmId algorithm,
                 const CryptoData& data,
                 blink::WebArrayBuffer* buffer);

// Preconditions:
//  * |key| is non-null.
//  * |hash| is a digest algorithm.
Status SignRsaSsaPkcs1v1_5(PrivateKey* key,
                           const blink::WebCryptoAlgorithm& hash,
                           const CryptoData& data,
                           blink::WebArrayBuffer* buffer);

// Preconditions:
//  * |key| is non-null.
//  * |hash| is a digest algorithm.
Status VerifyRsaSsaPkcs1v1_5(PublicKey* key,
                             const blink::WebCryptoAlgorithm& hash,
                             const CryptoData& signature,
                             const CryptoData& data,
                             bool* signature_match);

// |keylen_bytes| is the desired length of the key in bits.
//
// Preconditions:
//  * algorithm.id() is for a symmetric key algorithm.
//  * keylen_bytes is non-zero (TODO(eroman): revisit this).
//  * For AES algorithms |keylen_bytes| is either 16, 24, or 32 bytes long.
Status GenerateSecretKey(const blink::WebCryptoAlgorithm& algorithm,
                         bool extractable,
                         blink::WebCryptoKeyUsageMask usage_mask,
                         unsigned keylen_bytes,
                         blink::WebCryptoKey* key);

// Preconditions:
//  * algorithm.id() is for an RSA algorithm.
//  * public_exponent, modulus_length_bits and hash_or_null are the same as what
//    is in algorithm. They are split out for convenience.
//  * hash_or_null.isNull() may be true if a hash is not applicable to the
//    algorithm
//  * modulus_length_bits is not 0
//  * public_exponent is not empty.
Status GenerateRsaKeyPair(const blink::WebCryptoAlgorithm& algorithm,
                          bool extractable,
                          blink::WebCryptoKeyUsageMask usage_mask,
                          unsigned int modulus_length_bits,
                          const CryptoData& public_exponent,
                          const blink::WebCryptoAlgorithm& hash,
                          blink::WebCryptoKey* public_key,
                          blink::WebCryptoKey* private_key);

// Preconditions:
//  * |key| is non-null.
//  * |algorithm.id()| is for a symmetric key algorithm.
//  * For AES algorithms |key_data| is either 16, 24, or 32 bytes long.
Status ImportKeyRaw(const blink::WebCryptoAlgorithm& algorithm,
                    const CryptoData& key_data,
                    bool extractable,
                    blink::WebCryptoKeyUsageMask usage_mask,
                    blink::WebCryptoKey* key);

// Preconditions:
//  * algorithm.id() is for an RSA algorithm.
Status ImportRsaPublicKey(const blink::WebCryptoAlgorithm& algorithm,
                          bool extractable,
                          blink::WebCryptoKeyUsageMask usage_mask,
                          const CryptoData& modulus_data,
                          const CryptoData& exponent_data,
                          blink::WebCryptoKey* key);

Status ImportKeySpki(const blink::WebCryptoAlgorithm& algorithm,
                     const CryptoData& key_data,
                     bool extractable,
                     blink::WebCryptoKeyUsageMask usage_mask,
                     blink::WebCryptoKey* key);

Status ImportKeyPkcs8(const blink::WebCryptoAlgorithm& algorithm,
                      const CryptoData& key_data,
                      bool extractable,
                      blink::WebCryptoKeyUsageMask usage_mask,
                      blink::WebCryptoKey* key);

// Preconditions:
//  * |key| is non-null.
Status ExportKeyRaw(SymKey* key, blink::WebArrayBuffer* buffer);

// Preconditions:
//  * |key| is non-null.
Status ExportKeySpki(PublicKey* key, blink::WebArrayBuffer* buffer);

// Preconditions:
//  * |wrapping_key| is non-null
//  * |key| is non-null
Status WrapSymKeyAesKw(SymKey* wrapping_key,
                       SymKey* key,
                       blink::WebArrayBuffer* buffer);

// Unwraps (decrypts) |wrapped_key_data| using AES-KW and places the results in
// a WebCryptoKey. Raw key data remains inside NSS. This function should be used
// when the input |wrapped_key_data| is known to result in symmetric raw key
// data after AES-KW decryption.
// Preconditions:
//  * |wrapping_key| is non-null
//  * |key| is non-null
//  * |wrapped_key_data| is at least 24 bytes and a multiple of 8 bytes
//  * |algorithm.id()| is for a symmetric key algorithm.
Status UnwrapSymKeyAesKw(const CryptoData& wrapped_key_data,
                         SymKey* wrapping_key,
                         const blink::WebCryptoAlgorithm& algorithm,
                         bool extractable,
                         blink::WebCryptoKeyUsageMask usage_mask,
                         blink::WebCryptoKey* key);

// Performs AES-KW decryption on the input |data|. This function should be used
// when the input |data| does not directly represent a key and should instead be
// interpreted as generic bytes.
// Preconditions:
//  * |key| is non-null
//  * |data| is at least 24 bytes and a multiple of 8 bytes
//  * |buffer| is non-null.
Status DecryptAesKw(SymKey* key,
                    const CryptoData& data,
                    blink::WebArrayBuffer* buffer);

// Preconditions:
//  * |wrapping_key| is non-null
//  * |key| is non-null
Status WrapSymKeyRsaEs(PublicKey* wrapping_key,
                       SymKey* key,
                       blink::WebArrayBuffer* buffer);

// Preconditions:
//  * |wrapping_key| is non-null
//  * |key| is non-null
//  * |algorithm.id()| is for a symmetric key algorithm.
Status UnwrapSymKeyRsaEs(const CryptoData& wrapped_key_data,
                         PrivateKey* wrapping_key,
                         const blink::WebCryptoAlgorithm& algorithm,
                         bool extractable,
                         blink::WebCryptoKeyUsageMask usage_mask,
                         blink::WebCryptoKey* key);

}  // namespace platform

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_CHILD_WEBCRYPTO_PLATFORM_CRYPTO_H_
