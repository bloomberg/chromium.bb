// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto/webcrypto_util.h"

#include "base/base64.h"
#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

namespace content {

namespace webcrypto {

namespace {

bool IsHashAlgorithm(blink::WebCryptoAlgorithmId alg_id) {
  return alg_id == blink::WebCryptoAlgorithmIdSha1 ||
         alg_id == blink::WebCryptoAlgorithmIdSha224 ||
         alg_id == blink::WebCryptoAlgorithmIdSha256 ||
         alg_id == blink::WebCryptoAlgorithmIdSha384 ||
         alg_id == blink::WebCryptoAlgorithmIdSha512;
}

}  // namespace

const uint8* Uint8VectorStart(const std::vector<uint8>& data) {
  if (data.empty())
    return NULL;
  return &data[0];
}

void ShrinkBuffer(blink::WebArrayBuffer* buffer, unsigned new_size) {
  DCHECK_LE(new_size, buffer->byteLength());

  if (new_size == buffer->byteLength())
    return;

  blink::WebArrayBuffer new_buffer = blink::WebArrayBuffer::create(new_size, 1);
  DCHECK(!new_buffer.isNull());
  memcpy(new_buffer.data(), buffer->data(), new_size);
  *buffer = new_buffer;
}

blink::WebArrayBuffer CreateArrayBuffer(const uint8* data, unsigned data_size) {
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

blink::WebCryptoAlgorithm GetInnerHashAlgorithm(
    const blink::WebCryptoAlgorithm& algorithm) {
  if (algorithm.hmacParams())
    return algorithm.hmacParams()->hash();
  if (algorithm.hmacKeyParams())
    return algorithm.hmacKeyParams()->hash();
  if (algorithm.rsaSsaParams())
    return algorithm.rsaSsaParams()->hash();
  if (algorithm.rsaOaepParams())
    return algorithm.rsaOaepParams()->hash();
  return blink::WebCryptoAlgorithm::createNull();
}

blink::WebCryptoAlgorithm CreateAlgorithm(blink::WebCryptoAlgorithmId id) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(id, NULL);
}

blink::WebCryptoAlgorithm CreateHmacAlgorithmByHashId(
    blink::WebCryptoAlgorithmId hash_id) {
  DCHECK(IsHashAlgorithm(hash_id));
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdHmac,
      new blink::WebCryptoHmacParams(CreateAlgorithm(hash_id)));
}

blink::WebCryptoAlgorithm CreateHmacKeyGenAlgorithm(
    blink::WebCryptoAlgorithmId hash_id,
    unsigned key_length_bytes) {
  DCHECK(IsHashAlgorithm(hash_id));
  // key_length_bytes == 0 means unspecified
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdHmac,
      new blink::WebCryptoHmacKeyParams(
          CreateAlgorithm(hash_id), (key_length_bytes != 0), key_length_bytes));
}

blink::WebCryptoAlgorithm CreateRsaSsaAlgorithm(
    blink::WebCryptoAlgorithmId hash_id) {
  DCHECK(IsHashAlgorithm(hash_id));
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      new blink::WebCryptoRsaSsaParams(CreateAlgorithm(hash_id)));
}

blink::WebCryptoAlgorithm CreateRsaOaepAlgorithm(
    blink::WebCryptoAlgorithmId hash_id) {
  DCHECK(IsHashAlgorithm(hash_id));
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdRsaOaep,
      new blink::WebCryptoRsaOaepParams(
          CreateAlgorithm(hash_id), false, NULL, 0));
}

blink::WebCryptoAlgorithm CreateRsaKeyGenAlgorithm(
    blink::WebCryptoAlgorithmId algorithm_id,
    unsigned modulus_length,
    const std::vector<uint8>& public_exponent) {
  DCHECK(algorithm_id == blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5 ||
         algorithm_id == blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 ||
         algorithm_id == blink::WebCryptoAlgorithmIdRsaOaep);
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      algorithm_id,
      new blink::WebCryptoRsaKeyGenParams(
          modulus_length,
          webcrypto::Uint8VectorStart(public_exponent),
          public_exponent.size()));
}

blink::WebCryptoAlgorithm CreateAesCbcAlgorithm(const std::vector<uint8>& iv) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdAesCbc,
      new blink::WebCryptoAesCbcParams(Uint8VectorStart(iv), iv.size()));
}

blink::WebCryptoAlgorithm CreateAesGcmAlgorithm(
    const std::vector<uint8>& iv,
    const std::vector<uint8>& additional_data,
    uint8 tag_length_bytes) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdAesCbc,
      new blink::WebCryptoAesGcmParams(Uint8VectorStart(iv),
                                       iv.size(),
                                       additional_data.size() != 0,
                                       Uint8VectorStart(additional_data),
                                       additional_data.size(),
                                       tag_length_bytes != 0,
                                       tag_length_bytes));
}

unsigned int ShaBlockSizeBytes(blink::WebCryptoAlgorithmId hash_id) {
  switch (hash_id) {
    case blink::WebCryptoAlgorithmIdSha1:
    case blink::WebCryptoAlgorithmIdSha224:
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

}  // namespace webcrypto

}  // namespace content
