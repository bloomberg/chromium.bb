// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/reliable_quic_stream.h"

#include "net/quic/quic_connection.h"
#include "net/quic/quic_spdy_compressor.h"
#include "net/quic/quic_spdy_decompressor.h"
#include "net/quic/quic_utils.h"
#include "net/quic/spdy_utils.h"
#include "net/quic/test_tools/quic_session_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::StringPiece;
using std::min;
using testing::_;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;
using testing::StrEq;
using testing::StrictMock;

namespace net {
namespace test {
namespace {

const char kData1[] = "FooAndBar";
const char kData2[] = "EepAndBaz";
const size_t kDataLen = 9;
const QuicGuid kGuid = 42;
const QuicGuid kStreamId = 3;
const bool kIsServer = true;
const bool kShouldProcessData = true;

class TestStream : public ReliableQuicStream {
 public:
  TestStream(QuicStreamId id,
                         QuicSession* session,
                         bool should_process_data)
      : ReliableQuicStream(id, session),
        should_process_data_(should_process_data) {
  }

  virtual uint32 ProcessData(const char* data, uint32 data_len) OVERRIDE {
    DVLOG(1) << "ProcessData data_len: " << data_len;
    data_ += string(data, data_len);
    return should_process_data_ ? data_len : 0;
  }

  using ReliableQuicStream::WriteData;
  using ReliableQuicStream::CloseReadSide;
  using ReliableQuicStream::CloseWriteSide;

  const string& data() const { return data_; }

 private:
  bool should_process_data_;
  string data_;
};

class ReliableQuicStreamTest : public ::testing::TestWithParam<bool> {
 public:
  ReliableQuicStreamTest() {
    headers_[":host"] = "www.google.com";
    headers_[":path"] = "/index.hml";
    headers_[":scheme"] = "https";
  }

  void Initialize(bool stream_should_process_data) {
    connection_ = new testing::StrictMock<MockConnection>(
        kGuid, IPEndPoint(), kIsServer);
    session_.reset(new testing::StrictMock<MockSession>(
        connection_, kIsServer));
    stream_.reset(new TestStream(kStreamId, session_.get(),
                                 stream_should_process_data));
    stream2_.reset(new TestStream(kStreamId + 2, session_.get(),
                                 stream_should_process_data));
    compressor_.reset(new QuicSpdyCompressor());
    decompressor_.reset(new QuicSpdyDecompressor);
    write_blocked_list_ =
        QuicSessionPeer::GetWriteblockedStreams(session_.get());
  }

 protected:
  MockConnection* connection_;
  scoped_ptr<MockSession> session_;
  scoped_ptr<TestStream> stream_;
  scoped_ptr<TestStream> stream2_;
  scoped_ptr<QuicSpdyCompressor> compressor_;
  scoped_ptr<QuicSpdyDecompressor> decompressor_;
  SpdyHeaderBlock headers_;
  BlockedList<QuicStreamId>* write_blocked_list_;
};

TEST_F(ReliableQuicStreamTest, WriteAllData) {
  Initialize(kShouldProcessData);

  connection_->options()->max_packet_length =
      1 + QuicPacketCreator::StreamFramePacketOverhead(
          1, PACKET_8BYTE_GUID, !kIncludeVersion,
          PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
  // TODO(rch): figure out how to get StrEq working here.
  //EXPECT_CALL(*session_, WriteData(kStreamId, StrEq(kData1), _, _)).WillOnce(
  EXPECT_CALL(*session_, WriteData(kStreamId, _, _, _)).WillOnce(
      Return(QuicConsumedData(kDataLen, true)));
  EXPECT_EQ(kDataLen, stream_->WriteData(kData1, false).bytes_consumed);
  EXPECT_TRUE(write_blocked_list_->IsEmpty());
}

// TODO(rtenneti): Death tests crash on OS_ANDROID.
#if GTEST_HAS_DEATH_TEST && !defined(NDEBUG) && !defined(OS_ANDROID)
TEST_F(ReliableQuicStreamTest, NoBlockingIfNoDataOrFin) {
  Initialize(kShouldProcessData);

  // Write no data and no fin.  If we consume nothing we should not be write
  // blocked.
  EXPECT_DEBUG_DEATH({
    EXPECT_CALL(*session_, WriteData(kStreamId, _, _, _)).WillOnce(
        Return(QuicConsumedData(0, false)));
    stream_->WriteData(StringPiece(), false);
  EXPECT_TRUE(write_blocked_list_->IsEmpty());
  }, "");
}
#endif  // GTEST_HAS_DEATH_TEST && !defined(NDEBUG) && !defined(OS_ANDROID)

TEST_F(ReliableQuicStreamTest, BlockIfOnlySomeDataConsumed) {
  Initialize(kShouldProcessData);

  // Write some data and no fin.  If we consume some but not all of the data,
  // we should be write blocked a not all the data was consumed.
  EXPECT_CALL(*session_, WriteData(kStreamId, _, _, _)).WillOnce(
      Return(QuicConsumedData(1, false)));
  stream_->WriteData(StringPiece(kData1, 2), false);
  ASSERT_EQ(1, write_blocked_list_->NumObjects());
}


TEST_F(ReliableQuicStreamTest, BlockIfFinNotConsumedWithData) {
  Initialize(kShouldProcessData);

  // Write some data and no fin.  If we consume all the data but not the fin,
  // we should be write blocked because the fin was not consumed.
  // (This should never actually happen as the fin should be sent out with the
  // last data)
  EXPECT_CALL(*session_, WriteData(kStreamId, _, _, _)).WillOnce(
      Return(QuicConsumedData(2, false)));
  stream_->WriteData(StringPiece(kData1, 2), true);
  ASSERT_EQ(1, write_blocked_list_->NumObjects());
}

TEST_F(ReliableQuicStreamTest, BlockIfSoloFinNotConsumed) {
  Initialize(kShouldProcessData);

  // Write no data and a fin.  If we consume nothing we should be write blocked,
  // as the fin was not consumed.
  EXPECT_CALL(*session_, WriteData(kStreamId, _, _, _)).WillOnce(
      Return(QuicConsumedData(0, false)));
  stream_->WriteData(StringPiece(), true);
  ASSERT_EQ(1, write_blocked_list_->NumObjects());
}

TEST_F(ReliableQuicStreamTest, WriteData) {
  Initialize(kShouldProcessData);

  EXPECT_TRUE(write_blocked_list_->IsEmpty());
  connection_->options()->max_packet_length =
      1 + QuicPacketCreator::StreamFramePacketOverhead(
          1, PACKET_8BYTE_GUID, !kIncludeVersion,
          PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
  // TODO(rch): figure out how to get StrEq working here.
  //EXPECT_CALL(*session_, WriteData(_, StrEq(kData1), _, _)).WillOnce(
  EXPECT_CALL(*session_, WriteData(_, _, _, _)).WillOnce(
      Return(QuicConsumedData(kDataLen - 1, false)));
  // The return will be kDataLen, because the last byte gets buffered.
  EXPECT_EQ(kDataLen, stream_->WriteData(kData1, false).bytes_consumed);
  EXPECT_FALSE(write_blocked_list_->IsEmpty());

  // Queue a bytes_consumed write.
  EXPECT_EQ(kDataLen, stream_->WriteData(kData2, false).bytes_consumed);

  // Make sure we get the tail of the first write followed by the bytes_consumed
  InSequence s;
  //EXPECT_CALL(*session_, WriteData(_, StrEq(&kData1[kDataLen - 1]), _, _)).
  EXPECT_CALL(*session_, WriteData(_, _, _, _)).
      WillOnce(Return(QuicConsumedData(1, false)));
  //EXPECT_CALL(*session_, WriteData(_, StrEq(kData2), _, _)).
  EXPECT_CALL(*session_, WriteData(_, _, _, _)).
      WillOnce(Return(QuicConsumedData(kDataLen - 2, false)));
  stream_->OnCanWrite();

  // And finally the end of the bytes_consumed
  //EXPECT_CALL(*session_, WriteData(_, StrEq(&kData2[kDataLen - 2]), _, _)).
  EXPECT_CALL(*session_, WriteData(_, _, _, _)).
      WillOnce(Return(QuicConsumedData(2, true)));
  stream_->OnCanWrite();
}

TEST_F(ReliableQuicStreamTest, ConnectionCloseAfterStreamClose) {
  Initialize(kShouldProcessData);

  stream_->CloseReadSide();
  stream_->CloseWriteSide();
  EXPECT_EQ(QUIC_STREAM_NO_ERROR, stream_->stream_error());
  EXPECT_EQ(QUIC_NO_ERROR, stream_->connection_error());
  stream_->ConnectionClose(QUIC_INTERNAL_ERROR, false);
  EXPECT_EQ(QUIC_STREAM_NO_ERROR, stream_->stream_error());
  EXPECT_EQ(QUIC_NO_ERROR, stream_->connection_error());
}

TEST_F(ReliableQuicStreamTest, ProcessHeaders) {
  Initialize(kShouldProcessData);

  string compressed_headers = compressor_->CompressHeaders(headers_);
  QuicStreamFrame frame(kStreamId, false, 0, compressed_headers);

  stream_->OnStreamFrame(frame);
  EXPECT_EQ(SpdyUtils::SerializeUncompressedHeaders(headers_), stream_->data());
}

TEST_F(ReliableQuicStreamTest, ProcessHeadersWithInvalidHeaderId) {
  Initialize(kShouldProcessData);

  string compressed_headers = compressor_->CompressHeaders(headers_);
  compressed_headers.replace(0, 1, 1, '\xFF');  // Illegal header id.
  QuicStreamFrame frame(kStreamId, false, 0, compressed_headers);

  EXPECT_CALL(*connection_, SendConnectionClose(QUIC_INVALID_HEADER_ID));
  stream_->OnStreamFrame(frame);
}

TEST_F(ReliableQuicStreamTest, ProcessHeadersAndBody) {
  Initialize(kShouldProcessData);

  string compressed_headers = compressor_->CompressHeaders(headers_);
  string body = "this is the body";
  string data = compressed_headers + body;
  QuicStreamFrame frame(kStreamId, false, 0, data);

  stream_->OnStreamFrame(frame);
  EXPECT_EQ(SpdyUtils::SerializeUncompressedHeaders(headers_) + body,
            stream_->data());
}

TEST_F(ReliableQuicStreamTest, ProcessHeadersAndBodyFragments) {
  Initialize(kShouldProcessData);

  string compressed_headers = compressor_->CompressHeaders(headers_);
  string body = "this is the body";
  string data = compressed_headers + body;

  for (size_t fragment_size = 1; fragment_size < data.size(); ++fragment_size) {
    Initialize(kShouldProcessData);
    for (size_t offset = 0; offset < data.size(); offset += fragment_size) {
      size_t remaining_data = data.length() - offset;
      StringPiece fragment(data.data() + offset,
                           min(fragment_size, remaining_data));
      QuicStreamFrame frame(kStreamId, false, offset, fragment);

      stream_->OnStreamFrame(frame);
    }
    ASSERT_EQ(SpdyUtils::SerializeUncompressedHeaders(headers_) + body,
              stream_->data()) << "fragment_size: " << fragment_size;
  }
}

TEST_F(ReliableQuicStreamTest, ProcessHeadersAndBodyReadv) {
  Initialize(!kShouldProcessData);

  string compressed_headers = compressor_->CompressHeaders(headers_);
  string body = "this is the body";
  string data = compressed_headers + body;
  QuicStreamFrame frame(kStreamId, false, 0, data);
  string uncompressed_headers =
      SpdyUtils::SerializeUncompressedHeaders(headers_);
  string uncompressed_data = uncompressed_headers + body;

  stream_->OnStreamFrame(frame);
  EXPECT_EQ(uncompressed_data, stream_->data());

  char buffer[1024];
  ASSERT_LT(data.length(), arraysize(buffer));
  struct iovec vec;
  vec.iov_base = buffer;
  vec.iov_len = arraysize(buffer);

  size_t bytes_read = stream_->Readv(&vec, 1);
  EXPECT_EQ(uncompressed_headers.length(), bytes_read);
  EXPECT_EQ(uncompressed_headers, string(buffer, bytes_read));

  bytes_read = stream_->Readv(&vec, 1);
  EXPECT_EQ(body.length(), bytes_read);
  EXPECT_EQ(body, string(buffer, bytes_read));
}

TEST_F(ReliableQuicStreamTest, ProcessHeadersAndBodyIncrementalReadv) {
  Initialize(!kShouldProcessData);

  string compressed_headers = compressor_->CompressHeaders(headers_);
  string body = "this is the body";
  string data = compressed_headers + body;
  QuicStreamFrame frame(kStreamId, false, 0, data);
  string uncompressed_headers =
      SpdyUtils::SerializeUncompressedHeaders(headers_);
  string uncompressed_data = uncompressed_headers + body;

  stream_->OnStreamFrame(frame);
  EXPECT_EQ(uncompressed_data, stream_->data());

  char buffer[1];
  struct iovec vec;
  vec.iov_base = buffer;
  vec.iov_len = arraysize(buffer);
  for (size_t i = 0; i < uncompressed_data.length(); ++i) {
    size_t bytes_read = stream_->Readv(&vec, 1);
    ASSERT_EQ(1u, bytes_read);
    EXPECT_EQ(uncompressed_data.data()[i], buffer[0]);
  }
}

TEST_F(ReliableQuicStreamTest, ProcessHeadersUsingReadvWithMultipleIovecs) {
  Initialize(!kShouldProcessData);

  string compressed_headers = compressor_->CompressHeaders(headers_);
  string body = "this is the body";
  string data = compressed_headers + body;
  QuicStreamFrame frame(kStreamId, false, 0, data);
  string uncompressed_headers =
      SpdyUtils::SerializeUncompressedHeaders(headers_);
  string uncompressed_data = uncompressed_headers + body;

  stream_->OnStreamFrame(frame);
  EXPECT_EQ(uncompressed_data, stream_->data());

  char buffer1[1];
  char buffer2[1];
  struct iovec vec[2];
  vec[0].iov_base = buffer1;
  vec[0].iov_len = arraysize(buffer1);
  vec[1].iov_base = buffer2;
  vec[1].iov_len = arraysize(buffer2);
  for (size_t i = 0; i < uncompressed_data.length(); i += 2) {
    size_t bytes_read = stream_->Readv(vec, 2);
    ASSERT_EQ(2u, bytes_read) << i;
    ASSERT_EQ(uncompressed_data.data()[i], buffer1[0]) << i;
    ASSERT_EQ(uncompressed_data.data()[i + 1], buffer2[0]) << i;
  }
}

TEST_F(ReliableQuicStreamTest, ProcessHeadersEarly) {
  Initialize(kShouldProcessData);

  string compressed_headers1 = compressor_->CompressHeaders(headers_);
  QuicStreamFrame frame1(stream_->id(), false, 0, compressed_headers1);
  string decompressed_headers1 =
      SpdyUtils::SerializeUncompressedHeaders(headers_);

  headers_["content-type"] = "text/plain";
  string compressed_headers2 = compressor_->CompressHeaders(headers_);
  QuicStreamFrame frame2(stream2_->id(), false, 0, compressed_headers2);
  string decompressed_headers2 =
      SpdyUtils::SerializeUncompressedHeaders(headers_);

  stream2_->OnStreamFrame(frame2);
  EXPECT_EQ("", stream_->data());

  stream_->OnStreamFrame(frame1);
  EXPECT_EQ(decompressed_headers1, stream_->data());

  EXPECT_EQ(2u, session_->decompressor()->current_header_id());
  stream2_->OnDecompressorAvailable();
  EXPECT_EQ(decompressed_headers2, stream2_->data());
}

TEST_F(ReliableQuicStreamTest, ProcessHeadersDelay) {
  Initialize(!kShouldProcessData);

  string compressed_headers = compressor_->CompressHeaders(headers_);
  QuicStreamFrame frame1(stream_->id(), false, 0, compressed_headers);
  string decompressed_headers =
      SpdyUtils::SerializeUncompressedHeaders(headers_);

  // Send the headers to the stream and verify they were decompressed.
  stream_->OnStreamFrame(frame1);
  EXPECT_EQ(2u, session_->decompressor()->current_header_id());

  // Verify that we are now able to handle the body data,
  // even though the stream has not processed the headers.
  EXPECT_CALL(*connection_, SendConnectionClose(QUIC_INVALID_HEADER_ID))
      .Times(0);
  QuicStreamFrame frame2(stream_->id(), false, compressed_headers.length(),
                         "body data");
  stream_->OnStreamFrame(frame2);
}

}  // namespace
}  // namespace test
}  // namespace net
