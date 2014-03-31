// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_RELIABLE_QUIC_STREAM_PEER_H_
#define NET_QUIC_TEST_TOOLS_RELIABLE_QUIC_STREAM_PEER_H_

#include "base/basictypes.h"
#include "net/quic/quic_protocol.h"

namespace net {

class ReliableQuicStream;

namespace test {

class ReliableQuicStreamPeer {
 public:
  static void SetWriteSideClosed(bool value, ReliableQuicStream* stream);
  static void SetStreamBytesWritten(QuicStreamOffset stream_bytes_written,
                                    ReliableQuicStream* stream);
  static void CloseReadSide(ReliableQuicStream* stream);

  static bool FinSent(ReliableQuicStream* stream);
  static bool RstSent(ReliableQuicStream* stream);

  static void SetFlowControlSendOffset(ReliableQuicStream* stream,
                                       QuicStreamOffset offset);
  static void SetFlowControlReceiveOffset(ReliableQuicStream* stream,
                                          QuicStreamOffset offset);
  static void SetFlowControlMaxReceiveWindow(ReliableQuicStream* stream,
                                             uint64 window_size);
  static QuicStreamOffset SendWindowOffset(ReliableQuicStream* stream);
  static uint64 SendWindowSize(ReliableQuicStream* stream);
  static QuicStreamOffset ReceiveWindowOffset(ReliableQuicStream* stream);
  static uint64 ReceiveWindowSize(ReliableQuicStream* stream);

  static uint32 SizeOfQueuedData(ReliableQuicStream* stream);

 private:
  DISALLOW_COPY_AND_ASSIGN(ReliableQuicStreamPeer);
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_RELIABLE_QUIC_STREAM_PEER_H_
