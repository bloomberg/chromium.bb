// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_session_peer.h"

#include "net/quic/quic_session.h"

namespace net {
namespace test {

// static
void QuicSessionPeer::SetNextStreamId(QuicStreamId id, QuicSession* session) {
  session->next_stream_id_ = id;
}

}  // namespace test
}  // namespace net
