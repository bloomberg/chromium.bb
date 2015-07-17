// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/gcm_message_cryptographer.h"

#include <openssl/aead.h>

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_util.h"

namespace gcm {

namespace {

// The BoringSSL functions used to seal (encrypt) and open (decrypt) a payload
// follow the same prototype, declared as follows.
using EVP_AEAD_CTX_TransformFunction =
    int(const EVP_AEAD_CTX *ctx, uint8_t *out, size_t *out_len,
        size_t max_out_len, const uint8_t *nonce, size_t nonce_len,
        const uint8_t *in, size_t in_len, const uint8_t *ad, size_t ad_len);

}  // namespace

bool GCMMessageCryptographer::EncryptDecryptRecordInternal(
    Mode mode,
    const base::StringPiece& input,
    const base::StringPiece& key,
    const base::StringPiece& nonce,
    std::string* output) const {
  DCHECK(output);

  const EVP_AEAD* aead = EVP_aead_aes_128_gcm();

  EVP_AEAD_CTX context;
  if (!EVP_AEAD_CTX_init(&context, aead,
                         reinterpret_cast<const uint8_t*>(key.data()),
                         key.size(), EVP_AEAD_DEFAULT_TAG_LENGTH, nullptr)) {
    return false;
  }

  base::CheckedNumeric<size_t> maximum_output_length(input.size());
  if (mode == ENCRYPT)
    maximum_output_length += kAuthenticationTagBytes;

  // WriteInto requires the buffer to finish with a NULL-byte.
  maximum_output_length += 1;

  size_t output_length = 0;
  uint8_t* raw_output = reinterpret_cast<uint8_t*>(
      base::WriteInto(output, maximum_output_length.ValueOrDie()));

  EVP_AEAD_CTX_TransformFunction* transform_function =
      mode == ENCRYPT ? EVP_AEAD_CTX_seal : EVP_AEAD_CTX_open;

  if (!transform_function(
          &context, raw_output, &output_length, output->size(),
          reinterpret_cast<const uint8_t*>(nonce.data()), nonce.size(),
          reinterpret_cast<const uint8_t*>(input.data()), input.size(),
          nullptr, 0)) {
    EVP_AEAD_CTX_cleanup(&context);
    return false;
  }

  EVP_AEAD_CTX_cleanup(&context);

  base::CheckedNumeric<size_t> expected_output_length(input.size());
  if (mode == ENCRYPT)
    expected_output_length += kAuthenticationTagBytes;
  else
    expected_output_length -= kAuthenticationTagBytes;

  DCHECK_EQ(expected_output_length.ValueOrDie(), output_length);

  output->resize(output_length);
  return true;
}

}  // namespace gcm
