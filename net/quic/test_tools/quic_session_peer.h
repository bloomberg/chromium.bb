// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_SESSION_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_SESSION_PEER_H_

#include "net/quic/quic_protocol.h"

namespace net {

class QuicSession;

namespace test {

class QuicSessionPeer {
 public:
  static void SetNextStreamId(QuicStreamId id, QuicSession* session);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicSessionPeer);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_SESSION_PEER_H_
