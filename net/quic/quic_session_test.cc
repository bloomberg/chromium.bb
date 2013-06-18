// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session.h"

#include <set>
#include <vector>

#include "base/containers/hash_tables.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/spdy/spdy_framer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::hash_map;
using std::set;
using std::vector;
using testing::_;
using testing::InSequence;

namespace net {
namespace test {
namespace {

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
    session()->config()->ToHandshakeMessage(&msg);
    const QuicErrorCode error = session()->config()->ProcessClientHello(
        msg, &error_details);
    EXPECT_EQ(QUIC_NO_ERROR, error);
    session()->OnCryptoHandshakeEvent(QuicSession::HANDSHAKE_CONFIRMED);
  }
};

class TestStream : public ReliableQuicStream {
 public:
  TestStream(QuicStreamId id, QuicSession* session)
      : ReliableQuicStream(id, session) {
  }

  using ReliableQuicStream::CloseWriteSide;

  virtual uint32 ProcessData(const char* data, uint32 data_len) {
    return data_len;
  }

  MOCK_METHOD0(OnCanWrite, void());
};

class TestSession : public QuicSession {
 public:
  TestSession(QuicConnection* connection, bool is_server)
      : QuicSession(connection, DefaultQuicConfig(), is_server),
        crypto_stream_(this) {
  }

  virtual QuicCryptoStream* GetCryptoStream() OVERRIDE {
    return &crypto_stream_;
  }

  virtual TestStream* CreateOutgoingReliableStream() OVERRIDE {
    TestStream* stream = new TestStream(GetNextStreamId(), this);
    ActivateStream(stream);
    return stream;
  }

  virtual TestStream* CreateIncomingReliableStream(QuicStreamId id) OVERRIDE {
    return new TestStream(id, this);
  }

  bool IsClosedStream(QuicStreamId id) {
    return QuicSession::IsClosedStream(id);
  }

  ReliableQuicStream* GetIncomingReliableStream(QuicStreamId stream_id) {
    return QuicSession::GetIncomingReliableStream(stream_id);
  }

  // Helper method for gmock
  void MarkTwoWriteBlocked() {
    this->MarkWriteBlocked(2);
  }

  TestCryptoStream crypto_stream_;
};

class QuicSessionTest : public ::testing::Test {
 protected:
  QuicSessionTest()
      : guid_(1),
        connection_(new MockConnection(guid_, IPEndPoint(), false)),
        session_(connection_, true) {
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

  QuicGuid guid_;
  MockConnection* connection_;
  TestSession session_;
  QuicConnectionVisitorInterface* visitor_;
  hash_map<QuicStreamId, ReliableQuicStream*>* streams_;
  set<QuicStreamId> closed_streams_;
};

TEST_F(QuicSessionTest, IsCryptoHandshakeConfirmed) {
  EXPECT_FALSE(session_.IsCryptoHandshakeConfirmed());
  CryptoHandshakeMessage message;
  session_.crypto_stream_.OnHandshakeMessage(message);
  EXPECT_TRUE(session_.IsCryptoHandshakeConfirmed());
}

TEST_F(QuicSessionTest, IsClosedStreamDefault) {
  // Ensure that no streams are initially closed.
  for (int i = kCryptoStreamId; i < 100; i++) {
    EXPECT_FALSE(session_.IsClosedStream(i));
  }
}

TEST_F(QuicSessionTest, IsClosedStreamLocallyCreated) {
  TestStream* stream2 = session_.CreateOutgoingReliableStream();
  EXPECT_EQ(2u, stream2->id());
  TestStream* stream4 = session_.CreateOutgoingReliableStream();
  EXPECT_EQ(4u, stream4->id());

  CheckClosedStreams();
  CloseStream(4);
  CheckClosedStreams();
  CloseStream(2);
  CheckClosedStreams();
}

TEST_F(QuicSessionTest, IsClosedStreamPeerCreated) {
  session_.GetIncomingReliableStream(3);
  session_.GetIncomingReliableStream(5);

  CheckClosedStreams();
  CloseStream(3);
  CheckClosedStreams();
  CloseStream(5);
  // Create stream id 9, and implicitly 7
  session_.GetIncomingReliableStream(9);
  CheckClosedStreams();
  // Close 9, but make sure 7 is still not closed
  CloseStream(9);
  CheckClosedStreams();
}

TEST_F(QuicSessionTest, StreamIdTooLarge) {
  session_.GetIncomingReliableStream(3);
  EXPECT_CALL(*connection_, SendConnectionClose(QUIC_INVALID_STREAM_ID));
  session_.GetIncomingReliableStream(105);
}

TEST_F(QuicSessionTest, DecompressionError) {
  ReliableQuicStream* stream = session_.GetIncomingReliableStream(3);
  EXPECT_CALL(*connection_, SendConnectionClose(QUIC_DECOMPRESSION_FAILURE));
  const char data[] =
      "\1\0\0\0"   // headers id
      "\0\0\0\4"   // length
      "abcd";      // invalid compressed data
  stream->ProcessRawData(data, arraysize(data));
}

TEST_F(QuicSessionTest, OnCanWrite) {
  TestStream* stream2 = session_.CreateOutgoingReliableStream();
  TestStream* stream4 = session_.CreateOutgoingReliableStream();
  TestStream* stream6 = session_.CreateOutgoingReliableStream();

  session_.MarkWriteBlocked(2);
  session_.MarkWriteBlocked(6);
  session_.MarkWriteBlocked(4);

  InSequence s;
  EXPECT_CALL(*stream2, OnCanWrite()).WillOnce(
      // Reregister, to test the loop limit.
      testing::InvokeWithoutArgs(&session_, &TestSession::MarkTwoWriteBlocked));
  EXPECT_CALL(*stream6, OnCanWrite());
  EXPECT_CALL(*stream4, OnCanWrite());

  EXPECT_FALSE(session_.OnCanWrite());
}

TEST_F(QuicSessionTest, OnCanWriteWithClosedStream) {
  TestStream* stream2 = session_.CreateOutgoingReliableStream();
  TestStream* stream4 = session_.CreateOutgoingReliableStream();
  session_.CreateOutgoingReliableStream();  // stream 6

  session_.MarkWriteBlocked(2);
  session_.MarkWriteBlocked(6);
  session_.MarkWriteBlocked(4);
  CloseStream(6);

  InSequence s;
  EXPECT_CALL(*stream2, OnCanWrite());
  EXPECT_CALL(*stream4, OnCanWrite());
  EXPECT_TRUE(session_.OnCanWrite());
}

// Regression test for http://crbug.com/248737
TEST_F(QuicSessionTest, OutOfOrderHeaders) {
  QuicSpdyCompressor compressor;
  SpdyHeaderBlock headers;
  headers[":host"] = "www.google.com";
  headers[":path"] = "/index.hml";
  headers[":scheme"] = "http";
  vector<QuicStreamFrame> frames;
  QuicPacketHeader header;
  header.public_header.guid = session_.guid();

  TestStream* stream2 = session_.CreateOutgoingReliableStream();
  TestStream* stream4 = session_.CreateOutgoingReliableStream();
  stream2->CloseWriteSide();
  stream4->CloseWriteSide();

  // Create frame with headers for stream2.
  string compressed_headers1 = compressor.CompressHeaders(headers);
  QuicStreamFrame frame1(stream2->id(), false, 0, compressed_headers1);

  // Create frame with headers for stream4.
  string compressed_headers2 = compressor.CompressHeaders(headers);
  QuicStreamFrame frame2(stream4->id(), true, 0, compressed_headers2);

  // Process the second frame first.  This will cause the headers to
  // be queued up and processed after the first frame is processed.
  frames.push_back(frame2);
  session_.OnPacket(IPEndPoint(), IPEndPoint(), header, frames);

  // Process the first frame, and un-cork the buffered headers.
  frames[0] = frame1;
  session_.OnPacket(IPEndPoint(), IPEndPoint(), header, frames);

  // Ensure that the streams actually close and we don't DCHECK.
  session_.ConnectionClose(QUIC_CONNECTION_TIMED_OUT, true);
}

TEST_F(QuicSessionTest, SendGoAway) {
  // After sending a GoAway, ensure new incoming streams cannot be created and
  // result in a RST being sent.
  EXPECT_CALL(*connection_,
              SendGoAway(QUIC_PEER_GOING_AWAY, 0u, "Going Away."));
  session_.SendGoAway(QUIC_PEER_GOING_AWAY, "Going Away.");
  EXPECT_TRUE(session_.goaway_sent());

  EXPECT_CALL(*connection_, SendRstStream(3u, QUIC_STREAM_PEER_GOING_AWAY));
  EXPECT_FALSE(session_.GetIncomingReliableStream(3u));
}

TEST_F(QuicSessionTest, IncreasedTimeoutAfterCryptoHandshake) {
  EXPECT_EQ(kDefaultInitialTimeoutSecs,
            QuicConnectionPeer::GetNetworkTimeout(connection_).ToSeconds());
  CryptoHandshakeMessage msg;
  session_.crypto_stream_.OnHandshakeMessage(msg);
  EXPECT_EQ(kDefaultTimeoutSecs,
            QuicConnectionPeer::GetNetworkTimeout(connection_).ToSeconds());
}

}  // namespace
}  // namespace test
}  // namespace net
