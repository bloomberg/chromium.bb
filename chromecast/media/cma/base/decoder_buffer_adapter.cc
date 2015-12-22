// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/decoder_buffer_adapter.h"

#include "chromecast/media/cma/base/cast_decrypt_config_impl.h"
#include "chromecast/public/media/cast_decrypt_config.h"
#include "media/base/decoder_buffer.h"

namespace chromecast {
namespace media {

DecoderBufferAdapter::DecoderBufferAdapter(
    const scoped_refptr<::media::DecoderBuffer>& buffer)
    : DecoderBufferAdapter(kPrimary, buffer) {
}

DecoderBufferAdapter::DecoderBufferAdapter(
    StreamId stream_id, const scoped_refptr<::media::DecoderBuffer>& buffer)
    : stream_id_(stream_id),
      buffer_(buffer) {
}

DecoderBufferAdapter::~DecoderBufferAdapter() {
}

StreamId DecoderBufferAdapter::stream_id() const {
  return stream_id_;
}

int64_t DecoderBufferAdapter::timestamp() const {
  return buffer_->timestamp().InMicroseconds();
}

void DecoderBufferAdapter::set_timestamp(base::TimeDelta timestamp) {
  buffer_->set_timestamp(timestamp);
}

const uint8_t* DecoderBufferAdapter::data() const {
  return buffer_->data();
}

uint8_t* DecoderBufferAdapter::writable_data() const {
  return buffer_->writable_data();
}

size_t DecoderBufferAdapter::data_size() const {
  return buffer_->data_size();
}

const CastDecryptConfig* DecoderBufferAdapter::decrypt_config() const {
  if (buffer_->decrypt_config() && !decrypt_config_) {
    const ::media::DecryptConfig* config = buffer_->decrypt_config();
    decrypt_config_.reset(new CastDecryptConfigImpl(*config));
  }

  return decrypt_config_.get();
}

bool DecoderBufferAdapter::end_of_stream() const {
  return buffer_->end_of_stream();
}

scoped_refptr<::media::DecoderBuffer>
DecoderBufferAdapter::ToMediaBuffer() const {
  return buffer_;
}

}  // namespace media
}  // namespace chromecast
