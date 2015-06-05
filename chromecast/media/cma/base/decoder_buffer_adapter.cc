// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/decoder_buffer_adapter.h"

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

base::TimeDelta DecoderBufferAdapter::timestamp() const {
  return buffer_->timestamp();
}

void DecoderBufferAdapter::set_timestamp(const base::TimeDelta& timestamp) {
  buffer_->set_timestamp(timestamp);
}

const uint8* DecoderBufferAdapter::data() const {
  return buffer_->data();
}

uint8* DecoderBufferAdapter::writable_data() const {
  return buffer_->writable_data();
}

size_t DecoderBufferAdapter::data_size() const {
  return buffer_->data_size();
}

const ::media::DecryptConfig* DecoderBufferAdapter::decrypt_config() const {
  return buffer_->decrypt_config();
}

bool DecoderBufferAdapter::end_of_stream() const {
  return buffer_->end_of_stream();
}

}  // namespace media
}  // namespace chromecast
