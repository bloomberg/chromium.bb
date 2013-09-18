// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_reliable_client_stream.h"

#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/quic/quic_client_session.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Return;
using testing::StrEq;
using testing::_;

namespace net {
namespace test {
namespace {

class MockDelegate : public QuicReliableClientStream::Delegate {
 public:
  MockDelegate() {}

  MOCK_METHOD0(OnSendData, int());
  MOCK_METHOD2(OnSendDataComplete, int(int, bool*));
  MOCK_METHOD2(OnDataReceived, int(const char*, int));
  MOCK_METHOD1(OnClose, void(QuicErrorCode));
  MOCK_METHOD1(OnError, void(int));
  MOCK_METHOD0(HasSendHeadersComplete, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

class QuicReliableClientStreamTest : public ::testing::Test {
 public:
  QuicReliableClientStreamTest()
      : session_(new MockConnection(1, IPEndPoint(), false), false),
        stream_(1, &session_, BoundNetLog()) {
    stream_.SetDelegate(&delegate_);
  }

  testing::StrictMock<MockDelegate> delegate_;
  MockSession session_;
  QuicReliableClientStream stream_;
  QuicCryptoClientConfig crypto_config_;
};

TEST_F(QuicReliableClientStreamTest, TerminateFromPeer) {
  EXPECT_CALL(delegate_, OnClose(QUIC_NO_ERROR));

  stream_.TerminateFromPeer(true);
}

TEST_F(QuicReliableClientStreamTest, ProcessData) {
  const char data[] = "hello world!";
  EXPECT_CALL(delegate_, OnDataReceived(StrEq(data), arraysize(data)));
  EXPECT_CALL(delegate_, OnClose(QUIC_NO_ERROR));

  EXPECT_EQ(arraysize(data), stream_.ProcessData(data, arraysize(data)));
}

TEST_F(QuicReliableClientStreamTest, ProcessDataWithError) {
  const char data[] = "hello world!";
  EXPECT_CALL(delegate_,
              OnDataReceived(StrEq(data),
                             arraysize(data))).WillOnce(Return(ERR_UNEXPECTED));
  EXPECT_CALL(delegate_, OnClose(QUIC_NO_ERROR));


  EXPECT_EQ(0u, stream_.ProcessData(data, arraysize(data)));
}

TEST_F(QuicReliableClientStreamTest, OnError) {
  EXPECT_CALL(delegate_, OnError(ERR_INTERNET_DISCONNECTED));

  stream_.OnError(ERR_INTERNET_DISCONNECTED);
  EXPECT_FALSE(stream_.GetDelegate());
}

TEST_F(QuicReliableClientStreamTest, WriteStreamData) {
  EXPECT_CALL(delegate_, OnClose(QUIC_NO_ERROR));

  const char kData1[] = "hello world";
  const size_t kDataLen = arraysize(kData1);

  // All data written.
  EXPECT_CALL(session_, WritevData(stream_.id(), _, _, _, _)).WillOnce(
      Return(QuicConsumedData(kDataLen, true)));
  TestCompletionCallback callback;
  EXPECT_EQ(OK, stream_.WriteStreamData(base::StringPiece(kData1, kDataLen),
                                        true, callback.callback()));
}

TEST_F(QuicReliableClientStreamTest, WriteStreamDataAsync) {
  EXPECT_CALL(delegate_, HasSendHeadersComplete());
  EXPECT_CALL(delegate_, OnClose(QUIC_NO_ERROR));

  const char kData1[] = "hello world";
  const size_t kDataLen = arraysize(kData1);

  // No data written.
  EXPECT_CALL(session_, WritevData(stream_.id(), _, _, _, _)).WillOnce(
      Return(QuicConsumedData(0, false)));
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING,
            stream_.WriteStreamData(base::StringPiece(kData1, kDataLen),
                                    true, callback.callback()));
  ASSERT_FALSE(callback.have_result());

  // All data written.
  EXPECT_CALL(session_, WritevData(stream_.id(), _, _, _, _)).WillOnce(
      Return(QuicConsumedData(kDataLen, true)));
  stream_.OnCanWrite();
  ASSERT_TRUE(callback.have_result());
  EXPECT_EQ(OK, callback.WaitForResult());
}

}  // namespace
}  // namespace test
}  // namespace net
