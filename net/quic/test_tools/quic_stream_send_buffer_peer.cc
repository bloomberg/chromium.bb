// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_stream_send_buffer_peer.h"

namespace net {

namespace test {

// static
void QuicStreamSendBufferPeer::SetStreamOffset(
    QuicStreamSendBuffer* send_buffer,
    QuicStreamOffset stream_offset) {
  send_buffer->stream_offset_ = stream_offset;
}

// static
const BufferedSlice* QuicStreamSendBufferPeer::CurrentWriteSlice(
    QuicStreamSendBuffer* send_buffer) {
  if (send_buffer->write_index_ == -1) {
    return nullptr;
  }
  return &send_buffer->buffered_slices_[send_buffer->write_index_];
}
}  // namespace test

}  // namespace net
