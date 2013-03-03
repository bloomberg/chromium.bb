// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/reliable_quic_stream.h"

#include "net/quic/quic_connection.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::StringPiece;
using testing::_;
using testing::InSequence;
using testing::Return;
using testing::StrEq;

namespace net {
namespace test {
namespace {

const char kData1[] = "FooAndBar";
const char kData2[] = "EepAndBaz";
const size_t kDataLen = 9;

class QuicReliableTestStream : public ReliableQuicStream {
 public:
  QuicReliableTestStream(QuicStreamId id, QuicSession* session)
      : ReliableQuicStream(id, session) {
  }
  virtual uint32 ProcessData(const char* data, uint32 data_len) OVERRIDE {
    return 0;
  }
  using ReliableQuicStream::WriteData;
};

class ReliableQuicStreamTest : public ::testing::TestWithParam<bool> {
 public:
  ReliableQuicStreamTest()
      : connection_(new MockConnection(1, IPEndPoint())),
        session_(connection_, true),
        stream_(1, &session_) {
  }

  MockConnection* connection_;
  MockSession session_;
  QuicReliableTestStream stream_;
};

TEST_F(ReliableQuicStreamTest, WriteAllData) {
  connection_->options()->max_packet_length =
      1 + QuicPacketCreator::StreamFramePacketOverhead(1, !kIncludeVersion);
  // TODO(rch): figure out how to get StrEq working here.
  //EXPECT_CALL(session_, WriteData(_, StrEq(kData1), _, _)).WillOnce(
  EXPECT_CALL(session_, WriteData(1, _, _, _)).WillOnce(
      Return(QuicConsumedData(kDataLen, true)));
  EXPECT_EQ(kDataLen, stream_.WriteData(kData1, false).bytes_consumed);
}

TEST_F(ReliableQuicStreamTest, WriteData) {
  connection_->options()->max_packet_length =
      1 + QuicPacketCreator::StreamFramePacketOverhead(1, !kIncludeVersion);
  // TODO(rch): figure out how to get StrEq working here.
  //EXPECT_CALL(session_, WriteData(_, StrEq(kData1), _, _)).WillOnce(
  EXPECT_CALL(session_, WriteData(_, _, _, _)).WillOnce(
      Return(QuicConsumedData(kDataLen - 1, false)));
  // The return will be kDataLen, because the last byte gets buffered.
  EXPECT_EQ(kDataLen, stream_.WriteData(kData1, false).bytes_consumed);

  // Queue a bytes_consumed write.
  EXPECT_EQ(kDataLen, stream_.WriteData(kData2, false).bytes_consumed);

  // Make sure we get the tail of the first write followed by the bytes_consumed
  InSequence s;
  //EXPECT_CALL(session_, WriteData(_, StrEq(&kData2[kDataLen - 1]), _, _)).
  EXPECT_CALL(session_, WriteData(_, _, _, _)).
      WillOnce(Return(QuicConsumedData(1, false)));
  //EXPECT_CALL(session_, WriteData(_, StrEq(kData2), _, _)).
  EXPECT_CALL(session_, WriteData(_, _, _, _)).
      WillOnce(Return(QuicConsumedData(kDataLen - 2, false)));
  stream_.OnCanWrite();

  // And finally the end of the bytes_consumed
  //EXPECT_CALL(session_, WriteData(_, StrEq(&kData2[kDataLen - 2]), _, _)).
  EXPECT_CALL(session_, WriteData(_, _, _, _)).
      WillOnce(Return(QuicConsumedData(2, true)));
  stream_.OnCanWrite();
}

}  // namespace
}  // namespace test
}  // namespace net
