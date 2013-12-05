// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEBCRYPTO_WEBCRYPTO_UTIL_H_
#define CONTENT_RENDERER_WEBCRYPTO_WEBCRYPTO_UTIL_H_

#include <map>
#include <string>
#include <vector>
#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"

namespace content {

namespace webcrypto {

// Returns a pointer to the start of |data|, or NULL if it is empty. This is a
// convenience function for getting the pointer, and should not be used beyond
// the expected lifetime of |data|.
CONTENT_EXPORT const uint8* Uint8VectorStart(const std::vector<uint8>& data);

// Shrinks a WebArrayBuffer to a new size.
// TODO(eroman): This works by re-allocating a new buffer. It would be better if
//               the WebArrayBuffer could just be truncated instead.
void ShrinkBuffer(blink::WebArrayBuffer* buffer, unsigned new_size);

// Creates a WebArrayBuffer from a uint8 byte array
blink::WebArrayBuffer CreateArrayBuffer(const uint8* data, unsigned data_size);

// This function decodes unpadded 'base64url' encoded data, as described in
// RFC4648 (http://www.ietf.org/rfc/rfc4648.txt) Section 5.
// In Web Crypto, this type of encoding is only used inside JWK.
bool Base64DecodeUrlSafe(const std::string& input, std::string* output);

// Returns the "hash" param for an algorithm if it exists, otherwise returns
// a null algorithm.
blink::WebCryptoAlgorithm GetInnerHashAlgorithm(
    const blink::WebCryptoAlgorithm& algorithm);

// Creates a WebCryptoAlgorithm without any parameters.
CONTENT_EXPORT blink::WebCryptoAlgorithm CreateAlgorithm(
    blink::WebCryptoAlgorithmId id);

// Creates an HMAC algorithm whose inner hash algorithm is determined by the
// specified hash output length. It is an error to call this method with an
// unsupported hash output length.
blink::WebCryptoAlgorithm CreateHmacAlgorithmByHashOutputLen(
    unsigned short hash_output_length_bits);

// Creates an HMAC algorithm whose inner hash algorithm is determined by the
// specified algorithm ID. It is an error to call this method with a hash
// algorithm that is not SHA*.
CONTENT_EXPORT blink::WebCryptoAlgorithm CreateHmacAlgorithmByHashId(
    blink::WebCryptoAlgorithmId hash_id);

// Creates an HMAC algorithm whose parameters struct is compatible with key
// generation. It is an error to call this with a hash_id that is not a SHA*.
// The key_length_bytes parameter is optional, with zero meaning unspecified.
CONTENT_EXPORT blink::WebCryptoAlgorithm CreateHmacKeyGenAlgorithm(
    blink::WebCryptoAlgorithmId hash_id,
    unsigned key_length_bytes);

// Creates an RSASSA-PKCS1-v1_5 algorithm. It is an error to call this with a
// hash_id that is not a SHA*.
blink::WebCryptoAlgorithm CreateRsaSsaAlgorithm(
    blink::WebCryptoAlgorithmId hash_id);

// Creates an RSA-OAEP algorithm. It is an error to call this with a hash_id
// that is not a SHA*.
blink::WebCryptoAlgorithm CreateRsaOaepAlgorithm(
    blink::WebCryptoAlgorithmId hash_id);

// Creates an RSA algorithm with ID algorithm_id, whose parameters struct is
// compatible with key generation.
CONTENT_EXPORT blink::WebCryptoAlgorithm CreateRsaKeyGenAlgorithm(
    blink::WebCryptoAlgorithmId algorithm_id,
    unsigned modulus_length,
    const std::vector<uint8>& public_exponent);

// Creates an AES-CBC algorithm.
CONTENT_EXPORT blink::WebCryptoAlgorithm CreateAesCbcAlgorithm(
    const std::vector<uint8>& iv);

// Creates and AES-GCM algorithm.
blink::WebCryptoAlgorithm CreateAesGcmAlgorithm(
    const std::vector<uint8>& iv,
    const std::vector<uint8>& additional_data,
    uint8 tag_length_bytes);

// Creates an AES-CBC algorithm whose parameters struct is compatible with key
// generation.
CONTENT_EXPORT blink::WebCryptoAlgorithm CreateAesCbcKeyGenAlgorithm(
    unsigned short key_length_bits);

// Creates an AES-GCM algorithm whose parameters struct is compatible with key
// generation.
blink::WebCryptoAlgorithm CreateAesGcmKeyGenAlgorithm(
    unsigned short key_length_bits);

}  // namespace webcrypto

}  // namespace content

#endif  // CONTENT_RENDERER_WEBCRYPTO_WEBCRYPTO_UTIL_H_
