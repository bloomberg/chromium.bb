// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/reliable_quic_stream_peer.h"

#include "net/quic/reliable_quic_stream.h"

namespace net {
namespace test {

// static
void ReliableQuicStreamPeer::SetWriteSideClosed(bool value,
                                                ReliableQuicStream* stream) {
  stream->write_side_closed_ = value;
}

// static
void ReliableQuicStreamPeer::SetOffset(QuicStreamOffset offset,
                                       ReliableQuicStream* stream) {
  stream->offset_ = offset;
}

}  // namespace test
}  // namespace net
