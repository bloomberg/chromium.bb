// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/decrypt_util.h"

#include <openssl/aes.h>
#include <stddef.h>
#include <stdint.h>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/public/media/cast_decrypt_config.h"
#include "crypto/symmetric_key.h"
#include "media/base/decoder_buffer.h"

namespace chromecast {
namespace media {

namespace {

class DecoderBufferClear : public DecoderBufferBase {
 public:
  explicit DecoderBufferClear(const scoped_refptr<DecoderBufferBase>& buffer);

  // DecoderBufferBase implementation.
  StreamId stream_id() const override;
  int64_t timestamp() const override;
  void set_timestamp(base::TimeDelta timestamp) override;
  const uint8_t* data() const override;
  uint8_t* writable_data() const override;
  size_t data_size() const override;
  const CastDecryptConfig* decrypt_config() const override;
  bool end_of_stream() const override;
  scoped_refptr<::media::DecoderBuffer> ToMediaBuffer() const override;

 private:
  ~DecoderBufferClear() override;

  scoped_refptr<DecoderBufferBase> const buffer_;

  DISALLOW_COPY_AND_ASSIGN(DecoderBufferClear);
};

DecoderBufferClear::DecoderBufferClear(
    const scoped_refptr<DecoderBufferBase>& buffer)
    : buffer_(buffer) {
}

DecoderBufferClear::~DecoderBufferClear() {
}

StreamId DecoderBufferClear::stream_id() const {
  return buffer_->stream_id();
}

int64_t DecoderBufferClear::timestamp() const {
  return buffer_->timestamp();
}

void DecoderBufferClear::set_timestamp(base::TimeDelta timestamp) {
  buffer_->set_timestamp(timestamp);
}

const uint8_t* DecoderBufferClear::data() const {
  return buffer_->data();
}

uint8_t* DecoderBufferClear::writable_data() const {
  return buffer_->writable_data();
}

size_t DecoderBufferClear::data_size() const {
  return buffer_->data_size();
}

const CastDecryptConfig* DecoderBufferClear::decrypt_config() const {
  // Buffer is clear so no decryption info.
  return NULL;
}

bool DecoderBufferClear::end_of_stream() const {
  return buffer_->end_of_stream();
}

scoped_refptr<::media::DecoderBuffer>
DecoderBufferClear::ToMediaBuffer() const {
  return buffer_->ToMediaBuffer();
}

}  // namespace

scoped_refptr<DecoderBufferBase> DecryptDecoderBuffer(
    const scoped_refptr<DecoderBufferBase>& buffer,
    crypto::SymmetricKey* key) {
  if (buffer->end_of_stream())
    return buffer;

  const CastDecryptConfig* decrypt_config = buffer->decrypt_config();
  if (!decrypt_config || decrypt_config->iv().size() == 0)
    return buffer;

  // Get the key.
  std::string raw_key;
  if (!key->GetRawKey(&raw_key)) {
    LOG(ERROR) << "Failed to get the underlying AES key";
    return buffer;
  }
  DCHECK_EQ(static_cast<int>(raw_key.length()), AES_BLOCK_SIZE);
  const uint8_t* key_u8 = reinterpret_cast<const uint8_t*>(raw_key.data());
  AES_KEY aes_key;
  if (AES_set_encrypt_key(key_u8, AES_BLOCK_SIZE * 8, &aes_key) != 0) {
    LOG(ERROR) << "Failed to set the AES key";
    return buffer;
  }

  // Get the IV.
  uint8_t aes_iv[AES_BLOCK_SIZE];
  DCHECK_EQ(static_cast<int>(decrypt_config->iv().length()),
            AES_BLOCK_SIZE);
  memcpy(aes_iv, decrypt_config->iv().data(), AES_BLOCK_SIZE);

  // Decryption state.
  unsigned int encrypted_byte_offset = 0;
  uint8_t ecount_buf[AES_BLOCK_SIZE];

  // Perform the decryption.
  const std::vector<SubsampleEntry>& subsamples = decrypt_config->subsamples();
  uint8_t* data = buffer->writable_data();
  uint32_t offset = 0;
  for (size_t k = 0; k < subsamples.size(); k++) {
    offset += subsamples[k].clear_bytes;
    uint32_t cypher_bytes = subsamples[k].cypher_bytes;
    CHECK_LE(static_cast<size_t>(offset + cypher_bytes), buffer->data_size());
    AES_ctr128_encrypt(
        data + offset, data + offset, cypher_bytes, &aes_key,
        aes_iv, ecount_buf, &encrypted_byte_offset);
    offset += cypher_bytes;
  }

  return scoped_refptr<DecoderBufferBase>(new DecoderBufferClear(buffer));
}

}  // namespace media
}  // namespace chromecast
