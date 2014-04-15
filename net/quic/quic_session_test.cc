// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session.h"

#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/containers/hash_tables.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/quic_crypto_stream.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"
#include "net/quic/reliable_quic_stream.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_data_stream_peer.h"
#include "net/quic/test_tools/quic_session_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/reliable_quic_stream_peer.h"
#include "net/spdy/spdy_framer.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::hash_map;
using std::set;
using std::vector;
using testing::_;
using testing::InSequence;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::StrictMock;

namespace net {
namespace test {
namespace {

const QuicPriority kHighestPriority = 0;
const QuicPriority kSomeMiddlePriority = 3;

class TestCryptoStream : public QuicCryptoStream {
 public:
  explicit TestCryptoStream(QuicSession* session)
      : QuicCryptoStream(session) {
  }

  virtual void OnHandshakeMessage(
      const CryptoHandshakeMessage& message) OVERRIDE {
    encryption_established_ = true;
    handshake_confirmed_ = true;
    CryptoHandshakeMessage msg;
    string error_details;
    session()->config()->set_peer_initial_flow_control_window_bytes(
        kInitialFlowControlWindowForTest);
    session()->config()->ToHandshakeMessage(&msg);
    const QuicErrorCode error = session()->config()->ProcessClientHello(
        msg, &error_details);
    EXPECT_EQ(QUIC_NO_ERROR, error);
    session()->OnConfigNegotiated();
    session()->OnCryptoHandshakeEvent(QuicSession::HANDSHAKE_CONFIRMED);
  }

  MOCK_METHOD0(OnCanWrite, void());
};

class TestStream : public QuicDataStream {
 public:
  TestStream(QuicStreamId id, QuicSession* session)
      : QuicDataStream(id, session) {
  }

  using ReliableQuicStream::CloseWriteSide;

  virtual uint32 ProcessData(const char* data, uint32 data_len) {
    return data_len;
  }

  void SendBody(const string& data, bool fin) {
    WriteOrBufferData(data, fin, NULL);
  }

  MOCK_METHOD0(OnCanWrite, void());
};

// Poor man's functor for use as callback in a mock.
class StreamBlocker {
 public:
  StreamBlocker(QuicSession* session, QuicStreamId stream_id)
      : session_(session),
        stream_id_(stream_id) {
  }

  void MarkWriteBlocked() {
    session_->MarkWriteBlocked(stream_id_, kSomeMiddlePriority);
  }

 private:
  QuicSession* const session_;
  const QuicStreamId stream_id_;
};

class TestSession : public QuicSession {
 public:
  explicit TestSession(QuicConnection* connection)
      : QuicSession(connection, DefaultQuicConfig()),
        crypto_stream_(this),
        writev_consumes_all_data_(false) {
  }

  virtual TestCryptoStream* GetCryptoStream() OVERRIDE {
    return &crypto_stream_;
  }

  virtual TestStream* CreateOutgoingDataStream() OVERRIDE {
    TestStream* stream = new TestStream(GetNextStreamId(), this);
    ActivateStream(stream);
    return stream;
  }

  virtual TestStream* CreateIncomingDataStream(QuicStreamId id) OVERRIDE {
    return new TestStream(id, this);
  }

  bool IsClosedStream(QuicStreamId id) {
    return QuicSession::IsClosedStream(id);
  }

  QuicDataStream* GetIncomingDataStream(QuicStreamId stream_id) {
    return QuicSession::GetIncomingDataStream(stream_id);
  }

  virtual QuicConsumedData WritevData(
      QuicStreamId id,
      const IOVector& data,
      QuicStreamOffset offset,
      bool fin,
      QuicAckNotifier::DelegateInterface* ack_notifier_delegate) OVERRIDE {
    // Always consumes everything.
    if (writev_consumes_all_data_) {
      return QuicConsumedData(data.TotalBufferSize(), fin);
    } else {
      return QuicSession::WritevData(id, data, offset, fin,
                                     ack_notifier_delegate);
    }
  }

  void set_writev_consumes_all_data(bool val) {
    writev_consumes_all_data_ = val;
  }

  QuicConsumedData SendStreamData() {
    return WritevData(5, IOVector(), 0, true, NULL);
  }

 private:
  TestCryptoStream crypto_stream_;

  bool writev_consumes_all_data_;
};

class QuicSessionTest : public ::testing::TestWithParam<QuicVersion> {
 protected:
  QuicSessionTest()
      : connection_(new MockConnection(true, SupportedVersions(GetParam()))),
        session_(connection_) {
    headers_[":host"] = "www.google.com";
    headers_[":path"] = "/index.hml";
    headers_[":scheme"] = "http";
    headers_["cookie"] =
        "__utma=208381060.1228362404.1372200928.1372200928.1372200928.1; "
        "__utmc=160408618; "
        "GX=DQAAAOEAAACWJYdewdE9rIrW6qw3PtVi2-d729qaa-74KqOsM1NVQblK4VhX"
        "hoALMsy6HOdDad2Sz0flUByv7etmo3mLMidGrBoljqO9hSVA40SLqpG_iuKKSHX"
        "RW3Np4bq0F0SDGDNsW0DSmTS9ufMRrlpARJDS7qAI6M3bghqJp4eABKZiRqebHT"
        "pMU-RXvTI5D5oCF1vYxYofH_l1Kviuiy3oQ1kS1enqWgbhJ2t61_SNdv-1XJIS0"
        "O3YeHLmVCs62O6zp89QwakfAWK9d3IDQvVSJzCQsvxvNIvaZFa567MawWlXg0Rh"
        "1zFMi5vzcns38-8_Sns; "
        "GA=v*2%2Fmem*57968640*47239936%2Fmem*57968640*47114716%2Fno-nm-"
        "yj*15%2Fno-cc-yj*5%2Fpc-ch*133685%2Fpc-s-cr*133947%2Fpc-s-t*1339"
        "47%2Fno-nm-yj*4%2Fno-cc-yj*1%2Fceft-as*1%2Fceft-nqas*0%2Fad-ra-c"
        "v_p%2Fad-nr-cv_p-f*1%2Fad-v-cv_p*859%2Fad-ns-cv_p-f*1%2Ffn-v-ad%"
        "2Fpc-t*250%2Fpc-cm*461%2Fpc-s-cr*722%2Fpc-s-t*722%2Fau_p*4"
        "SICAID=AJKiYcHdKgxum7KMXG0ei2t1-W4OD1uW-ecNsCqC0wDuAXiDGIcT_HA2o1"
        "3Rs1UKCuBAF9g8rWNOFbxt8PSNSHFuIhOo2t6bJAVpCsMU5Laa6lewuTMYI8MzdQP"
        "ARHKyW-koxuhMZHUnGBJAM1gJODe0cATO_KGoX4pbbFxxJ5IicRxOrWK_5rU3cdy6"
        "edlR9FsEdH6iujMcHkbE5l18ehJDwTWmBKBzVD87naobhMMrF6VvnDGxQVGp9Ir_b"
        "Rgj3RWUoPumQVCxtSOBdX0GlJOEcDTNCzQIm9BSfetog_eP_TfYubKudt5eMsXmN6"
        "QnyXHeGeK2UINUzJ-D30AFcpqYgH9_1BvYSpi7fc7_ydBU8TaD8ZRxvtnzXqj0RfG"
        "tuHghmv3aD-uzSYJ75XDdzKdizZ86IG6Fbn1XFhYZM-fbHhm3mVEXnyRW4ZuNOLFk"
        "Fas6LMcVC6Q8QLlHYbXBpdNFuGbuZGUnav5C-2I_-46lL0NGg3GewxGKGHvHEfoyn"
        "EFFlEYHsBQ98rXImL8ySDycdLEFvBPdtctPmWCfTxwmoSMLHU2SCVDhbqMWU5b0yr"
        "JBCScs_ejbKaqBDoB7ZGxTvqlrB__2ZmnHHjCr8RgMRtKNtIeuZAo ";
  }

  void CheckClosedStreams() {
    for (int i = kCryptoStreamId; i < 100; i++) {
      if (closed_streams_.count(i) == 0) {
        EXPECT_FALSE(session_.IsClosedStream(i)) << " stream id: " << i;
      } else {
        EXPECT_TRUE(session_.IsClosedStream(i)) << " stream id: " << i;
      }
    }
  }

  void CloseStream(QuicStreamId id) {
    session_.CloseStream(id);
    closed_streams_.insert(id);
  }

  QuicVersion version() const { return connection_->version(); }

  MockConnection* connection_;
  TestSession session_;
  set<QuicStreamId> closed_streams_;
  SpdyHeaderBlock headers_;
};

INSTANTIATE_TEST_CASE_P(Tests, QuicSessionTest,
                        ::testing::ValuesIn(QuicSupportedVersions()));

TEST_P(QuicSessionTest, PeerAddress) {
  EXPECT_EQ(IPEndPoint(Loopback4(), kTestPort), session_.peer_address());
}

TEST_P(QuicSessionTest, IsCryptoHandshakeConfirmed) {
  EXPECT_FALSE(session_.IsCryptoHandshakeConfirmed());
  CryptoHandshakeMessage message;
  session_.GetCryptoStream()->OnHandshakeMessage(message);
  EXPECT_TRUE(session_.IsCryptoHandshakeConfirmed());
}

TEST_P(QuicSessionTest, IsClosedStreamDefault) {
  // Ensure that no streams are initially closed.
  for (int i = kCryptoStreamId; i < 100; i++) {
    EXPECT_FALSE(session_.IsClosedStream(i)) << "stream id: " << i;
  }
}

TEST_P(QuicSessionTest, ImplicitlyCreatedStreams) {
  ASSERT_TRUE(session_.GetIncomingDataStream(7) != NULL);
  // Both 3 and 5 should be implicitly created.
  EXPECT_FALSE(session_.IsClosedStream(3));
  EXPECT_FALSE(session_.IsClosedStream(5));
  ASSERT_TRUE(session_.GetIncomingDataStream(5) != NULL);
  ASSERT_TRUE(session_.GetIncomingDataStream(3) != NULL);
}

TEST_P(QuicSessionTest, IsClosedStreamLocallyCreated) {
  TestStream* stream2 = session_.CreateOutgoingDataStream();
  EXPECT_EQ(2u, stream2->id());
  TestStream* stream4 = session_.CreateOutgoingDataStream();
  EXPECT_EQ(4u, stream4->id());

  CheckClosedStreams();
  CloseStream(4);
  CheckClosedStreams();
  CloseStream(2);
  CheckClosedStreams();
}

TEST_P(QuicSessionTest, IsClosedStreamPeerCreated) {
  QuicStreamId stream_id1 = 5;
  QuicStreamId stream_id2 = stream_id1 + 2;
  QuicDataStream* stream1 = session_.GetIncomingDataStream(stream_id1);
  QuicDataStreamPeer::SetHeadersDecompressed(stream1, true);
  QuicDataStream* stream2 = session_.GetIncomingDataStream(stream_id2);
  QuicDataStreamPeer::SetHeadersDecompressed(stream2, true);

  CheckClosedStreams();
  CloseStream(stream_id1);
  CheckClosedStreams();
  CloseStream(stream_id2);
  // Create a stream explicitly, and another implicitly.
  QuicDataStream* stream3 = session_.GetIncomingDataStream(stream_id2 + 4);
  QuicDataStreamPeer::SetHeadersDecompressed(stream3, true);
  CheckClosedStreams();
  // Close one, but make sure the other is still not closed
  CloseStream(stream3->id());
  CheckClosedStreams();
}

TEST_P(QuicSessionTest, StreamIdTooLarge) {
  QuicStreamId stream_id = 5;
  session_.GetIncomingDataStream(stream_id);
  EXPECT_CALL(*connection_, SendConnectionClose(QUIC_INVALID_STREAM_ID));
  session_.GetIncomingDataStream(stream_id + kMaxStreamIdDelta + 2);
}

TEST_P(QuicSessionTest, DecompressionError) {
  QuicHeadersStream* stream = QuicSessionPeer::GetHeadersStream(&session_);
  const unsigned char data[] = {
    0x80, 0x03, 0x00, 0x01,  // SPDY/3 SYN_STREAM frame
    0x00, 0x00, 0x00, 0x25,  // flags/length
    0x00, 0x00, 0x00, 0x05,  // stream id
    0x00, 0x00, 0x00, 0x00,  // associated stream id
    0x00, 0x00,
    'a',  'b',  'c',  'd'    // invalid compressed data
  };
  EXPECT_CALL(*connection_,
              SendConnectionCloseWithDetails(QUIC_INVALID_HEADERS_STREAM_DATA,
                                             "SPDY framing error."));
  stream->ProcessRawData(reinterpret_cast<const char*>(data),
                         arraysize(data));
}

TEST_P(QuicSessionTest, DebugDFatalIfMarkingClosedStreamWriteBlocked) {
  TestStream* stream2 = session_.CreateOutgoingDataStream();
  // Close the stream.
  stream2->Reset(QUIC_BAD_APPLICATION_PAYLOAD);
  // TODO(rtenneti): enable when chromium supports EXPECT_DEBUG_DFATAL.
  /*
  QuicStreamId kClosedStreamId = stream2->id();
  EXPECT_DEBUG_DFATAL(
      session_.MarkWriteBlocked(kClosedStreamId, kSomeMiddlePriority),
      "Marking unknown stream 2 blocked.");
  */
}

TEST_P(QuicSessionTest, DebugDFatalIfMarkWriteBlockedCalledWithWrongPriority) {
  const QuicPriority kDifferentPriority = 0;

  TestStream* stream2 = session_.CreateOutgoingDataStream();
  EXPECT_NE(kDifferentPriority, stream2->EffectivePriority());
  // TODO(rtenneti): enable when chromium supports EXPECT_DEBUG_DFATAL.
  /*
  EXPECT_DEBUG_DFATAL(
      session_.MarkWriteBlocked(stream2->id(), kDifferentPriority),
      "Priorities do not match.  Got: 0 Expected: 3");
  */
}

TEST_P(QuicSessionTest, OnCanWrite) {
  TestStream* stream2 = session_.CreateOutgoingDataStream();
  TestStream* stream4 = session_.CreateOutgoingDataStream();
  TestStream* stream6 = session_.CreateOutgoingDataStream();

  session_.MarkWriteBlocked(stream2->id(), kSomeMiddlePriority);
  session_.MarkWriteBlocked(stream6->id(), kSomeMiddlePriority);
  session_.MarkWriteBlocked(stream4->id(), kSomeMiddlePriority);

  InSequence s;
  StreamBlocker stream2_blocker(&session_, stream2->id());
  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce(
      // Reregister, to test the loop limit.
      InvokeWithoutArgs(&stream2_blocker, &StreamBlocker::MarkWriteBlocked));
  EXPECT_CALL(*stream6, OnCanWrite());
  EXPECT_CALL(*stream4, OnCanWrite());
  session_.OnCanWrite();
  EXPECT_TRUE(session_.HasPendingWrites());
}

TEST_P(QuicSessionTest, OnCanWriteBundlesStreams) {
  // Drive congestion control manually.
  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  QuicConnectionPeer::SetSendAlgorithm(session_.connection(), send_algorithm);

  TestStream* stream2 = session_.CreateOutgoingDataStream();
  TestStream* stream4 = session_.CreateOutgoingDataStream();
  TestStream* stream6 = session_.CreateOutgoingDataStream();

  session_.MarkWriteBlocked(stream2->id(), kSomeMiddlePriority);
  session_.MarkWriteBlocked(stream6->id(), kSomeMiddlePriority);
  session_.MarkWriteBlocked(stream4->id(), kSomeMiddlePriority);


  EXPECT_CALL(*send_algorithm, TimeUntilSend(_, _)).WillRepeatedly(
      Return(QuicTime::Delta::Zero()));
  EXPECT_CALL(*send_algorithm, GetCongestionWindow()).WillOnce(
      Return(kMaxPacketSize * 10));
  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce(IgnoreResult(
      InvokeWithoutArgs(&session_, &TestSession::SendStreamData)));
  EXPECT_CALL(*stream6, OnCanWrite()).WillOnce(IgnoreResult(
      InvokeWithoutArgs(&session_, &TestSession::SendStreamData)));
  EXPECT_CALL(*stream4, OnCanWrite()).WillOnce(IgnoreResult(
      InvokeWithoutArgs(&session_, &TestSession::SendStreamData)));
  MockPacketWriter* writer =
      static_cast<MockPacketWriter*>(
          QuicConnectionPeer::GetWriter(session_.connection()));
  EXPECT_CALL(*writer, WritePacket(_, _, _, _)).WillOnce(
                  Return(WriteResult(WRITE_STATUS_OK, 0)));
  EXPECT_CALL(*send_algorithm, OnPacketSent(_, _, _, _));
  session_.OnCanWrite();
  EXPECT_FALSE(session_.HasPendingWrites());
}

TEST_P(QuicSessionTest, OnCanWriteCongestionControlBlocks) {
  InSequence s;

  // Drive congestion control manually.
  MockSendAlgorithm* send_algorithm = new StrictMock<MockSendAlgorithm>;
  QuicConnectionPeer::SetSendAlgorithm(session_.connection(), send_algorithm);

  TestStream* stream2 = session_.CreateOutgoingDataStream();
  TestStream* stream4 = session_.CreateOutgoingDataStream();
  TestStream* stream6 = session_.CreateOutgoingDataStream();

  session_.MarkWriteBlocked(stream2->id(), kSomeMiddlePriority);
  session_.MarkWriteBlocked(stream6->id(), kSomeMiddlePriority);
  session_.MarkWriteBlocked(stream4->id(), kSomeMiddlePriority);

  StreamBlocker stream2_blocker(&session_, stream2->id());
  EXPECT_CALL(*send_algorithm, TimeUntilSend(_, _)).WillOnce(Return(
      QuicTime::Delta::Zero()));
  EXPECT_CALL(*stream2, OnCanWrite());
  EXPECT_CALL(*send_algorithm, TimeUntilSend(_, _)).WillOnce(Return(
      QuicTime::Delta::Zero()));
  EXPECT_CALL(*stream6, OnCanWrite());
  EXPECT_CALL(*send_algorithm, TimeUntilSend(_, _)).WillOnce(Return(
      QuicTime::Delta::Infinite()));
  // stream4->OnCanWrite is not called.

  session_.OnCanWrite();
  EXPECT_TRUE(session_.HasPendingWrites());

  // Still congestion-control blocked.
  EXPECT_CALL(*send_algorithm, TimeUntilSend(_, _)).WillOnce(Return(
      QuicTime::Delta::Infinite()));
  session_.OnCanWrite();
  EXPECT_TRUE(session_.HasPendingWrites());

  // stream4->OnCanWrite is called once the connection stops being
  // congestion-control blocked.
  EXPECT_CALL(*send_algorithm, TimeUntilSend(_, _)).WillOnce(Return(
      QuicTime::Delta::Zero()));
  EXPECT_CALL(*stream4, OnCanWrite());
  session_.OnCanWrite();
  EXPECT_FALSE(session_.HasPendingWrites());
}

TEST_P(QuicSessionTest, BufferedHandshake) {
  EXPECT_FALSE(session_.HasPendingHandshake());  // Default value.

  // Test that blocking other streams does not change our status.
  TestStream* stream2 = session_.CreateOutgoingDataStream();
  StreamBlocker stream2_blocker(&session_, stream2->id());
  stream2_blocker.MarkWriteBlocked();
  EXPECT_FALSE(session_.HasPendingHandshake());

  TestStream* stream3 = session_.CreateOutgoingDataStream();
  StreamBlocker stream3_blocker(&session_, stream3->id());
  stream3_blocker.MarkWriteBlocked();
  EXPECT_FALSE(session_.HasPendingHandshake());

  // Blocking (due to buffering of) the Crypto stream is detected.
  session_.MarkWriteBlocked(kCryptoStreamId, kHighestPriority);
  EXPECT_TRUE(session_.HasPendingHandshake());

  TestStream* stream4 = session_.CreateOutgoingDataStream();
  StreamBlocker stream4_blocker(&session_, stream4->id());
  stream4_blocker.MarkWriteBlocked();
  EXPECT_TRUE(session_.HasPendingHandshake());

  InSequence s;
  // Force most streams to re-register, which is common scenario when we block
  // the Crypto stream, and only the crypto stream can "really" write.

  // Due to prioritization, we *should* be asked to write the crypto stream
  // first.
  // Don't re-register the crypto stream (which signals complete writing).
  TestCryptoStream* crypto_stream = session_.GetCryptoStream();
  EXPECT_CALL(*crypto_stream, OnCanWrite());

  // Re-register all other streams, to show they weren't able to proceed.
  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce(
      InvokeWithoutArgs(&stream2_blocker, &StreamBlocker::MarkWriteBlocked));

  EXPECT_CALL(*stream3, OnCanWrite()).WillOnce(
      InvokeWithoutArgs(&stream3_blocker, &StreamBlocker::MarkWriteBlocked));

  EXPECT_CALL(*stream4, OnCanWrite()).WillOnce(
      InvokeWithoutArgs(&stream4_blocker, &StreamBlocker::MarkWriteBlocked));

  session_.OnCanWrite();
  EXPECT_TRUE(session_.HasPendingWrites());
  EXPECT_FALSE(session_.HasPendingHandshake());  // Crypto stream wrote.
}

TEST_P(QuicSessionTest, OnCanWriteWithClosedStream) {
  TestStream* stream2 = session_.CreateOutgoingDataStream();
  TestStream* stream4 = session_.CreateOutgoingDataStream();
  TestStream* stream6 = session_.CreateOutgoingDataStream();

  session_.MarkWriteBlocked(stream2->id(), kSomeMiddlePriority);
  session_.MarkWriteBlocked(stream6->id(), kSomeMiddlePriority);
  session_.MarkWriteBlocked(stream4->id(), kSomeMiddlePriority);
  CloseStream(stream6->id());

  InSequence s;
  EXPECT_CALL(*stream2, OnCanWrite());
  EXPECT_CALL(*stream4, OnCanWrite());
  session_.OnCanWrite();
  EXPECT_FALSE(session_.HasPendingWrites());
}

TEST_P(QuicSessionTest, SendGoAway) {
  EXPECT_CALL(*connection_,
              SendGoAway(QUIC_PEER_GOING_AWAY, 0u, "Going Away."));
  session_.SendGoAway(QUIC_PEER_GOING_AWAY, "Going Away.");
  EXPECT_TRUE(session_.goaway_sent());

  EXPECT_CALL(*connection_,
              SendRstStream(3u, QUIC_STREAM_PEER_GOING_AWAY, 0)).Times(0);
  EXPECT_TRUE(session_.GetIncomingDataStream(3u));
}

TEST_P(QuicSessionTest, DoNotSendGoAwayTwice) {
  EXPECT_CALL(*connection_,
              SendGoAway(QUIC_PEER_GOING_AWAY, 0u, "Going Away.")).Times(1);
  session_.SendGoAway(QUIC_PEER_GOING_AWAY, "Going Away.");
  EXPECT_TRUE(session_.goaway_sent());
  session_.SendGoAway(QUIC_PEER_GOING_AWAY, "Going Away.");
}

TEST_P(QuicSessionTest, IncreasedTimeoutAfterCryptoHandshake) {
  EXPECT_EQ(kDefaultInitialTimeoutSecs,
            QuicConnectionPeer::GetNetworkTimeout(connection_).ToSeconds());
  CryptoHandshakeMessage msg;
  session_.GetCryptoStream()->OnHandshakeMessage(msg);
  EXPECT_EQ(kDefaultTimeoutSecs,
            QuicConnectionPeer::GetNetworkTimeout(connection_).ToSeconds());
}

TEST_P(QuicSessionTest, RstStreamBeforeHeadersDecompressed) {
  QuicStreamId stream_id1 = 5;
  // Send two bytes of payload.
  QuicStreamFrame data1(stream_id1, false, 0, MakeIOVector("HT"));
  vector<QuicStreamFrame> frames;
  frames.push_back(data1);
  EXPECT_TRUE(session_.OnStreamFrames(frames));
  EXPECT_EQ(1u, session_.GetNumOpenStreams());

  QuicRstStreamFrame rst1(stream_id1, QUIC_STREAM_NO_ERROR, 0);
  session_.OnRstStream(rst1);
  EXPECT_EQ(0u, session_.GetNumOpenStreams());
}

TEST_P(QuicSessionTest, MultipleRstStreamsCauseSingleConnectionClose) {
  // If multiple invalid reset stream frames arrive in a single packet, this
  // should trigger a connection close. However there is no need to send
  // multiple connection close frames.

  // Create valid stream.
  const QuicStreamId kStreamId = 5;
  QuicStreamFrame data1(kStreamId, false, 0, MakeIOVector("HT"));
  vector<QuicStreamFrame> frames;
  frames.push_back(data1);
  EXPECT_TRUE(session_.OnStreamFrames(frames));
  EXPECT_EQ(1u, session_.GetNumOpenStreams());

  // Process first invalid stream reset, resulting in the connection being
  // closed.
  EXPECT_CALL(*connection_, SendConnectionClose(QUIC_INVALID_STREAM_ID))
      .Times(1);
  QuicStreamId kLargeInvalidStreamId = 99999999;
  QuicRstStreamFrame rst1(kLargeInvalidStreamId, QUIC_STREAM_NO_ERROR, 0);
  session_.OnRstStream(rst1);
  QuicConnectionPeer::CloseConnection(connection_);

  // Processing of second invalid stream reset should not result in the
  // connection being closed for a second time.
  QuicRstStreamFrame rst2(kLargeInvalidStreamId, QUIC_STREAM_NO_ERROR, 0);
  session_.OnRstStream(rst2);
}

TEST_P(QuicSessionTest, HandshakeUnblocksFlowControlBlockedStream) {
  // Test that if a stream is flow control blocked, then on receipt of the SHLO
  // containing a suitable send window offset, the stream becomes unblocked.
  if (version() < QUIC_VERSION_17) {
    return;
  }
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_stream_flow_control, true);

  // Ensure that Writev consumes all the data it is given (simulate no socket
  // blocking).
  session_.set_writev_consumes_all_data(true);

  // Create a stream, and send enough data to make it flow control blocked.
  TestStream* stream2 = session_.CreateOutgoingDataStream();
  string body(kDefaultFlowControlSendWindow, '.');
  EXPECT_FALSE(stream2->IsFlowControlBlocked());
  stream2->SendBody(body, false);
  EXPECT_TRUE(stream2->IsFlowControlBlocked());

  // Now complete the crypto handshake, resulting in an increased flow control
  // send window.
  CryptoHandshakeMessage msg;
  session_.GetCryptoStream()->OnHandshakeMessage(msg);

  // Stream is now unblocked.
  EXPECT_FALSE(stream2->IsFlowControlBlocked());
}

TEST_P(QuicSessionTest, InvalidFlowControlWindowInHandshake) {
  // Test that receipt of an invalid (< default) flow control window from peer
  // results in the connection being torn down.
  if (version() < QUIC_VERSION_17) {
    return;
  }
  ValueRestore<bool> old_flag(&FLAGS_enable_quic_stream_flow_control, true);

  uint32 kInvalidWindow = kDefaultFlowControlSendWindow - 1;

  CryptoHandshakeMessage msg;
  string error_details;
  session_.config()->set_peer_initial_flow_control_window_bytes(kInvalidWindow);
  session_.config()->ToHandshakeMessage(&msg);
  const QuicErrorCode error =
      session_.config()->ProcessClientHello(msg, &error_details);
  EXPECT_EQ(QUIC_NO_ERROR, error);

  EXPECT_CALL(*connection_, SendConnectionClose(QUIC_FLOW_CONTROL_ERROR));
  string expected_error("Peer sent us an invalid flow control send window: ");
  expected_error.append(reinterpret_cast<const char*>(&kInvalidWindow),
                        sizeof(kInvalidWindow));
  EXPECT_DFATAL(session_.OnConfigNegotiated(), expected_error);
}

}  // namespace
}  // namespace test
}  // namespace net
