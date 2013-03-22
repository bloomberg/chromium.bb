// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_client_session.h"

#include <vector>

#include "base/stl_util.h"
#include "net/base/capturing_net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/test_completion_callback.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"

using testing::_;

namespace net {
namespace test {
namespace {

const char kServerHostname[] = "www.example.com";

class QuicClientSessionTest : public ::testing::Test {
 protected:
  QuicClientSessionTest()
      : guid_(1),
        connection_(new PacketSavingConnection(guid_, IPEndPoint(), false)),
        session_(connection_, NULL, NULL, NULL, kServerHostname, &net_log_) {
  }

  void CompleteCryptoHandshake() {
    ASSERT_EQ(ERR_IO_PENDING,
              session_.CryptoConnect(callback_.callback()));
    CryptoTestUtils::HandshakeWithFakeServer(
        connection_, session_.GetCryptoStream());
    ASSERT_EQ(OK, callback_.WaitForResult());
  }

  QuicGuid guid_;
  PacketSavingConnection* connection_;
  CapturingNetLog net_log_;
  QuicClientSession session_;
  MockClock clock_;
  MockRandom random_;
  QuicConnectionVisitorInterface* visitor_;
  TestCompletionCallback callback_;
};

TEST_F(QuicClientSessionTest, CryptoConnect) {
  CompleteCryptoHandshake();
}

TEST_F(QuicClientSessionTest, MaxNumConnections) {
  CompleteCryptoHandshake();

  std::vector<QuicReliableClientStream*> streams;
  for (size_t i = 0; i < kDefaultMaxStreamsPerConnection; i++) {
    QuicReliableClientStream* stream = session_.CreateOutgoingReliableStream();
    EXPECT_TRUE(stream);
    streams.push_back(stream);
  }
  EXPECT_FALSE(session_.CreateOutgoingReliableStream());

  // Close a stream and ensure I can now open a new one.
  session_.CloseStream(streams[0]->id());
  EXPECT_TRUE(session_.CreateOutgoingReliableStream());
}

TEST_F(QuicClientSessionTest, GoAwayReceived) {
  // Initialize crypto before the client session will create a stream.
  ASSERT_TRUE(session_.CryptoConnect(callback_.callback()));
  // Simulate the server crypto handshake.
  CryptoHandshakeMessage server_message;
  server_message.tag = kSHLO;
  session_.GetCryptoStream()->OnHandshakeMessage(server_message);

  // After receiving a GoAway, I should no longer be able to create outgoing
  // streams.
  session_.OnGoAway(QuicGoAwayFrame(QUIC_PEER_GOING_AWAY, 1u, "Going away."));
  EXPECT_EQ(NULL, session_.CreateOutgoingReliableStream());
}

TEST_F(QuicClientSessionTest, Logging) {
  CompleteCryptoHandshake();

  // TODO(rch): Add some helper methods to simplify packet creation in tests.
  // Receive a packet, and verify that it was logged.
  QuicFramer framer(kQuicVersion1,
                    QuicDecrypter::Create(kNULL),
                    QuicEncrypter::Create(kNULL),
                    false);
  QuicRstStreamFrame frame;
  frame.stream_id = 2;
  frame.error_code = QUIC_CONNECTION_TIMED_OUT;
  frame.error_details = "doh!";

  QuicFrames frames;
  frames.push_back(QuicFrame(&frame));
  QuicPacketHeader header;
  header.public_header.guid = 1;
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.packet_sequence_number = 1;
  header.entropy_flag = false;
  header.fec_flag = false;
  header.fec_entropy_flag = false;
  header.fec_group = 0;
  scoped_ptr<QuicPacket> p(
      framer.ConstructFrameDataPacket(header, frames).packet);
  scoped_ptr<QuicEncryptedPacket> packet(framer.EncryptPacket(1, *p));
  IPAddressNumber ip;
  CHECK(ParseIPLiteralToNumber("192.0.2.33", &ip));
  IPEndPoint peer_addr = IPEndPoint(ip, 443);
  IPEndPoint self_addr = IPEndPoint(ip, 8435);

  connection_->ProcessUdpPacketInternal(self_addr, peer_addr, *packet);

  // Check that the NetLog was filled reasonably.
  net::CapturingNetLog::CapturedEntryList entries;
  net_log_.GetEntries(&entries);
  EXPECT_LT(0u, entries.size());

  // Check that we logged a QUIC_SESSION_PACKET_RECEIVED.
  int pos = net::ExpectLogContainsSomewhere(
      entries, 0,
      net::NetLog::TYPE_QUIC_SESSION_PACKET_RECEIVED,
      net::NetLog::PHASE_NONE);
  EXPECT_LT(0, pos);

  // ... and also a QUIC_SESSION_RST_STREAM_FRAME_RECEIVED.
  pos = net::ExpectLogContainsSomewhere(
      entries, 0,
      net::NetLog::TYPE_QUIC_SESSION_RST_STREAM_FRAME_RECEIVED,
      net::NetLog::PHASE_NONE);
  EXPECT_LT(0, pos);

  int stream_id;
  ASSERT_TRUE(entries[pos].GetIntegerValue("stream_id", &stream_id));
  EXPECT_EQ(frame.stream_id, static_cast<QuicStreamId>(stream_id));
  int error_code;
  ASSERT_TRUE(entries[pos].GetIntegerValue("error_code", &error_code));
  EXPECT_EQ(frame.error_code, static_cast<QuicErrorCode>(error_code));
  std::string details;
  ASSERT_TRUE(entries[pos].GetStringValue("details", &details));
  EXPECT_EQ(frame.error_details, details);
}

}  // namespace
}  // namespace test
}  // namespace net
