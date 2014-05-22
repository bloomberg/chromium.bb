// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_WEBCRYPTO_PLATFORM_CRYPTO_H_
#define CONTENT_CHILD_WEBCRYPTO_PLATFORM_CRYPTO_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebCrypto.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

namespace blink {
template <typename T>
class WebVector;
}

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
//
// -----------------
// Threading:
// -----------------
//
// Unless otherwise noted, functions in webcrypto::platform are called
// exclusively from a sequenced worker pool.
//
// This means that operations using a given key cannot occur in
// parallel and it is not necessary to guard against concurrent usage.
//
// The exceptions are:
//
//   * Key::ThreadSafeSerializeForClone(), which is called from the
//     target Blink thread during structured clone.
//
//   * ImportKeyRaw(), ImportKeySpki(), ImportKeyPkcs8(), which can be
//     called from the target Blink thread during structured clone
//     deserialization, as well as from the webcrypto worker pool.
//
//     TODO(eroman): Change it so import happens in worker pool too.
//                   http://crbug.com/366834
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

  virtual bool ThreadSafeSerializeForClone(
      blink::WebVector<uint8>* key_data) = 0;
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
                            std::vector<uint8>* buffer);

// Preconditions:
//  * |key| is a non-null AES-GCM key.
//  * |tag_length_bits| is one of {32, 64, 96, 104, 112, 120, 128}
Status EncryptDecryptAesGcm(EncryptOrDecrypt mode,
                            SymKey* key,
                            const CryptoData& data,
                            const CryptoData& iv,
                            const CryptoData& additional_data,
                            unsigned int tag_length_bits,
                            std::vector<uint8>* buffer);

// Preconditions:
//  * |key| is non-null.
//  * |data| is not empty.
Status EncryptRsaEsPkcs1v1_5(PublicKey* key,
                             const CryptoData& data,
                             std::vector<uint8>* buffer);

// Preconditions:
//  * |key| is non-null.
Status DecryptRsaEsPkcs1v1_5(PrivateKey* key,
                             const CryptoData& data,
                             std::vector<uint8>* buffer);

// Preconditions:
//  * |key| is non-null
//  * |hash| is a digest algorithm
//  * |label| MAY be empty (e.g. 0 bytes long).
Status EncryptRsaOaep(PublicKey* key,
                      const blink::WebCryptoAlgorithm& hash,
                      const CryptoData& label,
                      const CryptoData& data,
                      std::vector<uint8>* buffer);

// Preconditions:
//   * |key| is non-null
//   * |hash| is a digest algorithm
//   * |label| MAY be empty (e.g. 0 bytes long).
Status DecryptRsaOaep(PrivateKey* key,
                      const blink::WebCryptoAlgorithm& hash,
                      const CryptoData& label,
                      const CryptoData& data,
                      std::vector<uint8>* buffer);

// Preconditions:
//  * |key| is a non-null HMAC key.
//  * |hash| is a digest algorithm.
Status SignHmac(SymKey* key,
                const blink::WebCryptoAlgorithm& hash,
                const CryptoData& data,
                std::vector<uint8>* buffer);

// Preconditions:
//  * |algorithm| is a SHA function.
Status DigestSha(blink::WebCryptoAlgorithmId algorithm,
                 const CryptoData& data,
                 std::vector<uint8>* buffer);

// Preconditions:
//  * |algorithm| is a SHA function.
scoped_ptr<blink::WebCryptoDigestor> CreateDigestor(
    blink::WebCryptoAlgorithmId algorithm);

// Preconditions:
//  * |key| is non-null.
//  * |hash| is a digest algorithm.
Status SignRsaSsaPkcs1v1_5(PrivateKey* key,
                           const blink::WebCryptoAlgorithm& hash,
                           const CryptoData& data,
                           std::vector<uint8>* buffer);

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
// Note that this may be called from target Blink thread.
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

// Preconditions:
//  * algorithm.id() is for an RSA algorithm.
//  * modulus, public_exponent, and private_exponent will be non-empty. The
//    others will either all be specified (non-empty), or all be unspecified
//    (empty).
Status ImportRsaPrivateKey(const blink::WebCryptoAlgorithm& algorithm,
                           bool extractable,
                           blink::WebCryptoKeyUsageMask usage_mask,
                           const CryptoData& modulus,
                           const CryptoData& public_exponent,
                           const CryptoData& private_exponent,
                           const CryptoData& prime1,
                           const CryptoData& prime2,
                           const CryptoData& exponent1,
                           const CryptoData& exponent2,
                           const CryptoData& coefficient,
                           blink::WebCryptoKey* key);

// Note that this may be called from target Blink thread.
Status ImportKeySpki(const blink::WebCryptoAlgorithm& algorithm,
                     const CryptoData& key_data,
                     bool extractable,
                     blink::WebCryptoKeyUsageMask usage_mask,
                     blink::WebCryptoKey* key);

// Note that this may be called from target Blink thread.
Status ImportKeyPkcs8(const blink::WebCryptoAlgorithm& algorithm,
                      const CryptoData& key_data,
                      bool extractable,
                      blink::WebCryptoKeyUsageMask usage_mask,
                      blink::WebCryptoKey* key);

// Preconditions:
//  * |key| is non-null.
Status ExportKeyRaw(SymKey* key, std::vector<uint8>* buffer);

// Preconditions:
//  * |key| is non-null.
Status ExportKeySpki(PublicKey* key, std::vector<uint8>* buffer);

// Preconditions:
//  * |key| is non-null.
Status ExportRsaPublicKey(PublicKey* key,
                          std::vector<uint8>* modulus,
                          std::vector<uint8>* public_exponent);

// Preconditions:
//  * |key| is non-null.
Status ExportRsaPrivateKey(PrivateKey* key,
                           std::vector<uint8>* modulus,
                           std::vector<uint8>* public_exponent,
                           std::vector<uint8>* private_exponent,
                           std::vector<uint8>* prime1,
                           std::vector<uint8>* prime2,
                           std::vector<uint8>* exponent1,
                           std::vector<uint8>* exponent2,
                           std::vector<uint8>* coefficient);

// Preconditions:
//  * |key| is non-null.
Status ExportKeyPkcs8(PrivateKey* key,
                      const blink::WebCryptoKeyAlgorithm& key_algorithm,
                      std::vector<uint8>* buffer);

// Preconditions:
//  * |key| is non-null
//  * |wrapping_key| is non-null
Status WrapSymKeyAesKw(SymKey* key,
                       SymKey* wrapping_key,
                       std::vector<uint8>* buffer);

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
                    std::vector<uint8>* buffer);

// Preconditions:
//  * |key| is non-null
//  * |wrapping_key| is non-null
Status WrapSymKeyRsaEs(SymKey* key,
                       PublicKey* wrapping_key,
                       std::vector<uint8>* buffer);

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
