// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_TEST_TOOLS_SIMPLE_QUIC_FRAMER_H_
#define NET_QUIC_TEST_TOOLS_SIMPLE_QUIC_FRAMER_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"

namespace net {

class CryptoHandshakeMessage;
struct QuicAckFrame;
class QuicConnection;
class QuicConnectionVisitorInterface;
class QuicPacketCreator;
class ReceiveAlgorithmInterface;
class SendAlgorithmInterface;

namespace test {

class SimpleFramerVisitor;

// Peer to make public a number of otherwise private QuicFramer methods.
class SimpleQuicFramer {
 public:
  SimpleQuicFramer();
  ~SimpleQuicFramer();

  bool ProcessPacket(const QuicEncryptedPacket& packet);
  bool ProcessPacket(const QuicPacket& packet);

  const QuicPacketHeader& header() const;
  size_t num_frames() const;
  const std::vector<QuicAckFrame>& ack_frames() const;
  const std::vector<QuicConnectionCloseFrame>& connection_close_frames() const;
  const std::vector<QuicCongestionFeedbackFrame>& feedback_frames() const;
  const std::vector<QuicGoAwayFrame>& goaway_frames() const;
  const std::vector<QuicRstStreamFrame>& rst_stream_frames() const;
  const std::vector<QuicStreamFrame>& stream_frames() const;
  const QuicFecData& fec_data() const;
  // HandshakeMessage returns the index'th stream frame as a freshly allocated
  // handshake message which the caller takes ownership of. If parsing fails
  // then NULL is returned.
  CryptoHandshakeMessage* HandshakeMessage(size_t index) const;

 private:
  QuicFramer framer_;
  scoped_ptr<SimpleFramerVisitor> visitor_;
  DISALLOW_COPY_AND_ASSIGN(SimpleQuicFramer);
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_SIMPLE_QUIC_FRAMER_H_
