// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/cast_decoder_buffer_impl.h"

namespace chromecast {
namespace media {

CastDecoderBufferImpl::CastDecoderBufferImpl(
    const scoped_refptr<DecoderBufferBase>& buffer)
    : buffer_(buffer) {}

CastDecoderBufferImpl::~CastDecoderBufferImpl() {}

StreamId CastDecoderBufferImpl::stream_id() const {
  return buffer_->stream_id();
}

int64_t CastDecoderBufferImpl::timestamp() const {
  return buffer_->timestamp().InMicroseconds();
}

const uint8* CastDecoderBufferImpl::data() const {
  return buffer_->data();
}

size_t CastDecoderBufferImpl::data_size() const {
  return buffer_->data_size();
}

const CastDecryptConfig* CastDecoderBufferImpl::decrypt_config() const {
  return buffer_->decrypt_config();
}

bool CastDecoderBufferImpl::end_of_stream() const {
  return buffer_->end_of_stream();
}

const scoped_refptr<DecoderBufferBase>& CastDecoderBufferImpl::buffer() const {
  return buffer_;
}

void CastDecoderBufferImpl::set_buffer(
    const scoped_refptr<DecoderBufferBase>& buffer) {
  buffer_ = buffer;
}

}  // namespace media
}  // namespace chromecast
