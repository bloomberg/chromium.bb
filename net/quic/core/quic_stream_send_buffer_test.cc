// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/quic_stream_send_buffer.h"

#include "net/quic/core/quic_data_writer.h"
#include "net/quic/core/quic_simple_buffer_allocator.h"
#include "net/quic/core/quic_utils.h"
#include "net/quic/platform/api/quic_flags.h"
#include "net/quic/platform/api/quic_test.h"
#include "net/quic/test_tools/quic_test_utils.h"

using std::string;

namespace net {
namespace test {
namespace {

struct iovec MakeIovec(QuicStringPiece data) {
  struct iovec iov = {const_cast<char*>(data.data()),
                      static_cast<size_t>(data.size())};
  return iov;
}

class QuicStreamSendBufferTest : public QuicTest {
 public:
  QuicStreamSendBufferTest()
      : send_buffer_(
            &allocator_,
            FLAGS_quic_reloadable_flag_quic_allow_multiple_acks_for_data2) {
    EXPECT_EQ(0u, send_buffer_.size());
    EXPECT_EQ(0u, send_buffer_.stream_bytes_written());
    EXPECT_EQ(0u, send_buffer_.stream_bytes_outstanding());
    string data1(1536, 'a');
    string data2 = string(256, 'b') + string(256, 'c');
    struct iovec iov[2];
    iov[0] = MakeIovec(QuicStringPiece(data1));
    iov[1] = MakeIovec(QuicStringPiece(data2));

    QuicMemSlice slice1(&allocator_, 1024);
    memset(const_cast<char*>(slice1.data()), 'c', 1024);
    QuicMemSlice slice2(&allocator_, 768);
    memset(const_cast<char*>(slice2.data()), 'c', 768);

    // Save all data.
    SetQuicFlag(&FLAGS_quic_send_buffer_max_data_slice_size, 1024);
    send_buffer_.SaveStreamData(iov, 2, 0, 2048);
    send_buffer_.SaveMemSlice(std::move(slice1));
    EXPECT_TRUE(slice1.empty());
    send_buffer_.SaveMemSlice(std::move(slice2));
    EXPECT_TRUE(slice2.empty());
    // Write all data.
    send_buffer_.OnStreamDataConsumed(3840);
    EXPECT_EQ(3840u, send_buffer_.stream_bytes_written());
    EXPECT_EQ(3840u, send_buffer_.stream_bytes_outstanding());

    EXPECT_EQ(4u, send_buffer_.size());
  }

  SimpleBufferAllocator allocator_;
  QuicStreamSendBuffer send_buffer_;
};

TEST_F(QuicStreamSendBufferTest, CopyDataToBuffer) {
  char buf[4000];
  QuicDataWriter writer(4000, buf, HOST_BYTE_ORDER);
  string copy1(1024, 'a');
  string copy2 = string(512, 'a') + string(256, 'b') + string(256, 'c');
  string copy3(1024, 'c');
  string copy4(768, 'c');

  ASSERT_TRUE(send_buffer_.WriteStreamData(0, 1024, &writer));
  EXPECT_EQ(copy1, QuicStringPiece(buf, 1024));
  ASSERT_TRUE(send_buffer_.WriteStreamData(1024, 1024, &writer));
  EXPECT_EQ(copy2, QuicStringPiece(buf + 1024, 1024));
  ASSERT_TRUE(send_buffer_.WriteStreamData(2048, 1024, &writer));
  EXPECT_EQ(copy3, QuicStringPiece(buf + 2048, 1024));
  ASSERT_TRUE(send_buffer_.WriteStreamData(2048, 768, &writer));
  EXPECT_EQ(copy4, QuicStringPiece(buf + 3072, 768));

  // Test data piece across boundries.
  QuicDataWriter writer2(4000, buf, HOST_BYTE_ORDER);
  string copy5 = string(536, 'a') + string(256, 'b') + string(232, 'c');
  ASSERT_TRUE(send_buffer_.WriteStreamData(1000, 1024, &writer2));
  EXPECT_EQ(copy5, QuicStringPiece(buf, 1024));
  ASSERT_TRUE(send_buffer_.WriteStreamData(2500, 1024, &writer2));
  EXPECT_EQ(copy3, QuicStringPiece(buf + 1024, 1024));

  // Invalid data copy.
  QuicDataWriter writer3(4000, buf, HOST_BYTE_ORDER);
  EXPECT_FALSE(send_buffer_.WriteStreamData(3000, 1024, &writer3));
  EXPECT_FALSE(send_buffer_.WriteStreamData(0, 4000, &writer3));
}

TEST_F(QuicStreamSendBufferTest, RemoveStreamFrame) {
  QuicByteCount newly_acked_length;
  EXPECT_TRUE(send_buffer_.OnStreamDataAcked(1024, 1024, &newly_acked_length));
  EXPECT_EQ(1024u, newly_acked_length);
  EXPECT_EQ(4u, send_buffer_.size());

  EXPECT_TRUE(send_buffer_.OnStreamDataAcked(2048, 1024, &newly_acked_length));
  EXPECT_EQ(1024u, newly_acked_length);
  EXPECT_EQ(4u, send_buffer_.size());

  EXPECT_TRUE(send_buffer_.OnStreamDataAcked(0, 1024, &newly_acked_length));
  EXPECT_EQ(1024u, newly_acked_length);

  // Send buffer is cleaned up in order.
  EXPECT_EQ(1u, send_buffer_.size());
  EXPECT_TRUE(send_buffer_.OnStreamDataAcked(3072, 768, &newly_acked_length));
  EXPECT_EQ(768u, newly_acked_length);
  EXPECT_EQ(0u, send_buffer_.size());
}

TEST_F(QuicStreamSendBufferTest, RemoveStreamFrameAcrossBoundries) {
  QuicByteCount newly_acked_length;
  EXPECT_TRUE(send_buffer_.OnStreamDataAcked(2024, 576, &newly_acked_length));
  EXPECT_EQ(576u, newly_acked_length);
  EXPECT_EQ(4u, send_buffer_.size());

  EXPECT_TRUE(send_buffer_.OnStreamDataAcked(0, 1000, &newly_acked_length));
  EXPECT_EQ(1000u, newly_acked_length);
  EXPECT_EQ(4u, send_buffer_.size());

  EXPECT_TRUE(send_buffer_.OnStreamDataAcked(1000, 1024, &newly_acked_length));
  EXPECT_EQ(1024u, newly_acked_length);
  // Send buffer is cleaned up in order.
  EXPECT_EQ(2u, send_buffer_.size());

  EXPECT_TRUE(send_buffer_.OnStreamDataAcked(2600, 1024, &newly_acked_length));
  EXPECT_EQ(1024u, newly_acked_length);
  EXPECT_EQ(1u, send_buffer_.size());

  EXPECT_TRUE(send_buffer_.OnStreamDataAcked(3624, 216, &newly_acked_length));
  EXPECT_EQ(216u, newly_acked_length);
  EXPECT_EQ(0u, send_buffer_.size());
}

TEST_F(QuicStreamSendBufferTest, AckStreamDataMultipleTimes) {
  if (!FLAGS_quic_reloadable_flag_quic_allow_multiple_acks_for_data2) {
    return;
  }
  QuicByteCount newly_acked_length;
  EXPECT_TRUE(send_buffer_.OnStreamDataAcked(100, 1500, &newly_acked_length));
  EXPECT_EQ(1500u, newly_acked_length);
  EXPECT_EQ(4u, send_buffer_.size());

  EXPECT_TRUE(send_buffer_.OnStreamDataAcked(2000, 500, &newly_acked_length));
  EXPECT_EQ(500u, newly_acked_length);
  EXPECT_EQ(4u, send_buffer_.size());

  EXPECT_TRUE(send_buffer_.OnStreamDataAcked(0, 2600, &newly_acked_length));
  EXPECT_EQ(600u, newly_acked_length);
  // Send buffer is cleaned up in order.
  EXPECT_EQ(2u, send_buffer_.size());

  EXPECT_TRUE(send_buffer_.OnStreamDataAcked(2200, 1640, &newly_acked_length));
  EXPECT_EQ(1240u, newly_acked_length);
  EXPECT_EQ(0u, send_buffer_.size());

  EXPECT_FALSE(send_buffer_.OnStreamDataAcked(4000, 100, &newly_acked_length));
}

}  // namespace
}  // namespace test
}  // namespace net
