// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_mem_slice_span_impl.h"

#include "net/quic/core/quic_stream_send_buffer.h"
#include "net/quic/platform/api/quic_bug_tracker.h"

namespace net {

QuicMemSliceSpanImpl::QuicMemSliceSpanImpl(
    const std::vector<scoped_refptr<IOBuffer>>* buffers,
    const std::vector<size_t>* lengths)
    : buffers_(buffers), lengths_(lengths) {
  QUIC_BUG_IF(buffers->size() != lengths->size())
      << " buffers size and lengths size are not equal";
}

QuicMemSliceSpanImpl::QuicMemSliceSpanImpl(const QuicMemSliceSpanImpl& other) =
    default;
QuicMemSliceSpanImpl& QuicMemSliceSpanImpl::operator=(
    const QuicMemSliceSpanImpl& other) = default;
QuicMemSliceSpanImpl::QuicMemSliceSpanImpl(QuicMemSliceSpanImpl&& other) =
    default;
QuicMemSliceSpanImpl& QuicMemSliceSpanImpl::operator=(
    QuicMemSliceSpanImpl&& other) = default;

QuicMemSliceSpanImpl::~QuicMemSliceSpanImpl() = default;

QuicByteCount QuicMemSliceSpanImpl::SaveMemSlicesInSendBuffer(
    QuicStreamSendBuffer* send_buffer) {
  size_t saved_length = 0;
  for (size_t i = 0; i < buffers_->size(); ++i) {
    saved_length += (*lengths_)[i];
    send_buffer->SaveMemSlice(
        QuicMemSlice(QuicMemSliceImpl((*buffers_)[i], (*lengths_)[i])));
  }
  return saved_length;
}

}  // namespace net
