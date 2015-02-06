// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_QUIC_CONFIG_PEER_H_
#define NET_QUIC_TEST_TOOLS_QUIC_CONFIG_PEER_H_

#include "net/quic/quic_config.h"
#include "net/quic/quic_protocol.h"

namespace net {

class QuicConfig;

namespace test {

class QuicConfigPeer {
 public:
  static void SetReceivedSocketReceiveBuffer(QuicConfig* config,
                                             uint32 receive_buffer_bytes);

  static void SetReceivedInitialStreamFlowControlWindow(QuicConfig* config,
                                                        uint32 window_bytes);

  static void SetReceivedInitialSessionFlowControlWindow(QuicConfig* config,
                                                         uint32 window_bytes);

  static void SetReceivedConnectionOptions(QuicConfig* config,
                                           const QuicTagVector& options);

  static void SetReceivedBytesForConnectionId(QuicConfig* config, uint32 bytes);

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicConfigPeer);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_CONFIG_PEER_H_
