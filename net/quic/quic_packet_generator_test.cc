// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_generator.h"

#include <string>

#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/simple_quic_framer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;
using testing::_;

namespace net {
namespace test {
namespace {

class MockDelegate : public QuicPacketGenerator::DelegateInterface {
 public:
  MockDelegate() {}
  virtual ~MockDelegate() {}

  MOCK_METHOD1(CanWrite, bool(bool is_retransmission));

  MOCK_METHOD0(CreateAckFrame, QuicAckFrame*());
  MOCK_METHOD0(CreateFeedbackFrame, QuicCongestionFeedbackFrame*());
  MOCK_METHOD1(OnSerializedPacket, bool(const SerializedPacket& packet));

  void SetCanWrite(bool can_write) {
    EXPECT_CALL(*this, CanWrite(false)).WillRepeatedly(Return(can_write));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

// Simple struct for describing the contents of a packet.
// Useful in conjunction with a SimpleQuicFrame for validating
// that a packet contains the expected frames.
struct PacketContents {
  PacketContents()
      : num_ack_frames(0),
        num_connection_close_frames(0),
        num_feedback_frames(0),
        num_goaway_frames(0),
        num_rst_stream_frames(0),
        num_stream_frames(0),
        fec_group(0) {
  }

  size_t num_ack_frames;
  size_t num_connection_close_frames;
  size_t num_feedback_frames;
  size_t num_goaway_frames;
  size_t num_rst_stream_frames;
  size_t num_stream_frames;

  QuicFecGroupNumber fec_group;
};

}  // namespace

class QuicPacketGeneratorTest : public ::testing::Test {
 protected:
  QuicPacketGeneratorTest()
      : framer_(kQuicVersion1,
                QuicDecrypter::Create(kNULL),
                QuicEncrypter::Create(kNULL)),
        creator_(42, &framer_, &random_),
        generator_(&delegate_, &creator_),
        packet_(0, NULL, 0, NULL),
        packet2_(0, NULL, 0, NULL),
        packet3_(0, NULL, 0, NULL),
        packet4_(0, NULL, 0, NULL),
        packet5_(0, NULL, 0, NULL) {
  }

  ~QuicPacketGeneratorTest() {
    delete packet_.packet;
    delete packet_.retransmittable_frames;
    delete packet2_.packet;
    delete packet2_.retransmittable_frames;
    delete packet3_.packet;
    delete packet3_.retransmittable_frames;
    delete packet4_.packet;
    delete packet4_.retransmittable_frames;
    delete packet5_.packet;
    delete packet5_.retransmittable_frames;
  }

  QuicAckFrame* CreateAckFrame() {
    // TODO(rch): Initialize this so it can be verified later.
    return new QuicAckFrame(0, 0);
  }

  QuicCongestionFeedbackFrame* CreateFeedbackFrame() {
    QuicCongestionFeedbackFrame* frame = new QuicCongestionFeedbackFrame;
    frame->type = kFixRate;
    frame->fix_rate.bitrate = QuicBandwidth::FromBytesPerSecond(42);
    return frame;
  }

  QuicRstStreamFrame* CreateRstStreamFrame() {
    return new QuicRstStreamFrame(1, QUIC_NO_ERROR);
  }

  QuicGoAwayFrame* CreateGoAwayFrame() {
    return new QuicGoAwayFrame(QUIC_NO_ERROR, 1, "");
  }

  void CheckPacketContains(const PacketContents& contents,
                           const SerializedPacket& packet) {
    size_t num_retransmittable_frames = contents.num_connection_close_frames +
        contents.num_goaway_frames + contents.num_rst_stream_frames +
        contents.num_stream_frames;
    size_t num_frames = contents.num_feedback_frames + contents.num_ack_frames +
        num_retransmittable_frames;

    if (num_retransmittable_frames == 0) {
      ASSERT_TRUE(packet.retransmittable_frames == NULL);
    } else {
      ASSERT_TRUE(packet.retransmittable_frames != NULL);
      EXPECT_EQ(num_retransmittable_frames,
                packet.retransmittable_frames->frames().size());
    }

    ASSERT_TRUE(packet.packet != NULL);
    ASSERT_TRUE(simple_framer_.ProcessPacket(*packet.packet));
    EXPECT_EQ(num_frames, simple_framer_.num_frames());
    EXPECT_EQ(contents.num_ack_frames, simple_framer_.ack_frames().size());
    EXPECT_EQ(contents.num_connection_close_frames,
              simple_framer_.connection_close_frames().size());
    EXPECT_EQ(contents.num_feedback_frames,
              simple_framer_.feedback_frames().size());
    EXPECT_EQ(contents.num_goaway_frames,
              simple_framer_.goaway_frames().size());
    EXPECT_EQ(contents.num_rst_stream_frames,
              simple_framer_.rst_stream_frames().size());
    EXPECT_EQ(contents.num_stream_frames,
              simple_framer_.stream_frames().size());
    EXPECT_EQ(contents.fec_group, simple_framer_.header().fec_group);
  }

  void CheckPacketHasSingleStreamFrame(const SerializedPacket& packet) {
    ASSERT_TRUE(packet.retransmittable_frames != NULL);
    EXPECT_EQ(1u, packet.retransmittable_frames->frames().size());
    ASSERT_TRUE(packet.packet != NULL);
    ASSERT_TRUE(simple_framer_.ProcessPacket(*packet.packet));
    EXPECT_EQ(1u, simple_framer_.num_frames());
    EXPECT_EQ(1u, simple_framer_.stream_frames().size());
  }

  void CheckPacketIsFec(const SerializedPacket& packet,
                        QuicPacketSequenceNumber fec_group) {
    ASSERT_TRUE(packet.retransmittable_frames == NULL);
    ASSERT_TRUE(packet.packet != NULL);
    ASSERT_TRUE(simple_framer_.ProcessPacket(*packet.packet));
    EXPECT_TRUE(simple_framer_.header().fec_flag);
    EXPECT_EQ(fec_group, simple_framer_.fec_data().fec_group);
  }

  StringPiece CreateData(size_t len) {
    data_array_.reset(new char[len]);
    memset(data_array_.get(), '?', len);
    return StringPiece(data_array_.get(), len);
  }

  QuicFramer framer_;
  MockRandom random_;
  QuicPacketCreator creator_;
  testing::StrictMock<MockDelegate> delegate_;
  QuicPacketGenerator generator_;
  SimpleQuicFramer simple_framer_;
  SerializedPacket packet_;
  SerializedPacket packet2_;
  SerializedPacket packet3_;
  SerializedPacket packet4_;
  SerializedPacket packet5_;

 private:
  scoped_ptr<char[]> data_array_;
};

TEST_F(QuicPacketGeneratorTest, ShouldSendAck_NotWritable) {
  delegate_.SetCanWrite(false);

  generator_.SetShouldSendAck(false);
  EXPECT_TRUE(generator_.HasQueuedData());
}

TEST_F(QuicPacketGeneratorTest, ShouldSendAck_WritableAndShouldNotFlush) {
  delegate_.SetCanWrite(true);
  generator_.StartBatchOperations();

  EXPECT_CALL(delegate_, CreateAckFrame()).WillOnce(Return(CreateAckFrame()));

  generator_.SetShouldSendAck(false);
  EXPECT_TRUE(generator_.HasQueuedData());
}

TEST_F(QuicPacketGeneratorTest, ShouldSendAck_WritableAndShouldFlush) {
  delegate_.SetCanWrite(true);

  EXPECT_CALL(delegate_, CreateAckFrame()).WillOnce(Return(CreateAckFrame()));
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));

  generator_.SetShouldSendAck(false);
  EXPECT_FALSE(generator_.HasQueuedData());

  PacketContents contents;
  contents.num_ack_frames = 1;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest,
       ShouldSendAckWithFeedback_WritableAndShouldNotFlush) {
  delegate_.SetCanWrite(true);
  generator_.StartBatchOperations();

  EXPECT_CALL(delegate_, CreateAckFrame()).WillOnce(Return(CreateAckFrame()));
  EXPECT_CALL(delegate_, CreateFeedbackFrame()).WillOnce(
      Return(CreateFeedbackFrame()));

  generator_.SetShouldSendAck(true);
  EXPECT_TRUE(generator_.HasQueuedData());
}

TEST_F(QuicPacketGeneratorTest,
       ShouldSendAckWithFeedback_WritableAndShouldFlush) {
  delegate_.SetCanWrite(true);

  EXPECT_CALL(delegate_, CreateAckFrame()).WillOnce(Return(CreateAckFrame()));
  EXPECT_CALL(delegate_, CreateFeedbackFrame()).WillOnce(
      Return(CreateFeedbackFrame()));

  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));

  generator_.SetShouldSendAck(true);
  EXPECT_FALSE(generator_.HasQueuedData());

  PacketContents contents;
  contents.num_ack_frames = 1;
  contents.num_feedback_frames = 1;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest, AddControlFrame_NotWritable) {
  delegate_.SetCanWrite(false);

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedData());
}

TEST_F(QuicPacketGeneratorTest, AddControlFrame_WritableAndShouldNotFlush) {
  delegate_.SetCanWrite(true);
  generator_.StartBatchOperations();

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedData());
}

TEST_F(QuicPacketGeneratorTest, AddControlFrame_WritableAndShouldFlush) {
  delegate_.SetCanWrite(true);

  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_FALSE(generator_.HasQueuedData());

  PacketContents contents;
  contents.num_rst_stream_frames = 1;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest, ConsumeData_NotWritable) {
  delegate_.SetCanWrite(false);

  QuicConsumedData consumed = generator_.ConsumeData(1, "foo", 2, true);
  EXPECT_EQ(0u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedData());
}

TEST_F(QuicPacketGeneratorTest, ConsumeData_WritableAndShouldNotFlush) {
  delegate_.SetCanWrite(true);
  generator_.StartBatchOperations();

  QuicConsumedData consumed = generator_.ConsumeData(1, "foo", 2, true);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedData());
}

TEST_F(QuicPacketGeneratorTest, ConsumeData_WritableAndShouldFlush) {
  delegate_.SetCanWrite(true);

  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));
  QuicConsumedData consumed = generator_.ConsumeData(1, "foo", 2, true);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedData());

  PacketContents contents;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest,
       ConsumeDataMultipleTimes_WritableAndShouldNotFlush) {
  delegate_.SetCanWrite(true);
  generator_.StartBatchOperations();

  generator_.ConsumeData(1, "foo", 2, true);
  QuicConsumedData consumed = generator_.ConsumeData(3, "quux", 7, false);
  EXPECT_EQ(4u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedData());
}

TEST_F(QuicPacketGeneratorTest, ConsumeData_BatchOperations) {
  delegate_.SetCanWrite(true);
  generator_.StartBatchOperations();

  generator_.ConsumeData(1, "foo", 2, true);
  QuicConsumedData consumed = generator_.ConsumeData(3, "quux", 7, false);
  EXPECT_EQ(4u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedData());

  // Now both frames will be flushed out.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));
  generator_.FinishBatchOperations();
  EXPECT_FALSE(generator_.HasQueuedData());

  PacketContents contents;
  contents.num_stream_frames = 2;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest, ConsumeDataFEC) {
  delegate_.SetCanWrite(true);

  // Send FEC every two packets.
  creator_.options()->max_packets_per_fec_group = 2;

  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet2_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet3_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet4_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet5_), Return(true)));
  }

  // Send enough data to create 3 packets: two full and one partial.
  size_t data_len = 2 * kMaxPacketSize + 100;
  QuicConsumedData consumed =
      generator_.ConsumeData(3, CreateData(data_len), 0, true);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedData());

  CheckPacketHasSingleStreamFrame(packet_);
  CheckPacketHasSingleStreamFrame(packet2_);
  CheckPacketIsFec(packet3_, 1);

  CheckPacketHasSingleStreamFrame(packet4_);
  CheckPacketIsFec(packet5_, 4);
}

TEST_F(QuicPacketGeneratorTest, ConsumeDataSendsFecAtEnd) {
  delegate_.SetCanWrite(true);

  // Send FEC every six packets.
  creator_.options()->max_packets_per_fec_group = 6;

  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet2_), Return(true)));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        DoAll(SaveArg<0>(&packet3_), Return(true)));
  }

  // Send enough data to create 2 packets: one full and one partial.
  size_t data_len = 1 * kMaxPacketSize + 100;
  QuicConsumedData consumed =
      generator_.ConsumeData(3, CreateData(data_len), 0, true);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedData());

  CheckPacketHasSingleStreamFrame(packet_);
  CheckPacketHasSingleStreamFrame(packet2_);
  CheckPacketIsFec(packet3_, 1);
}

TEST_F(QuicPacketGeneratorTest, NotWritableThenBatchOperations) {
  delegate_.SetCanWrite(false);

  generator_.SetShouldSendAck(true);
  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedData());

  delegate_.SetCanWrite(true);

  generator_.StartBatchOperations();

  // When the first write operation is invoked, the ack and feedback
  // frames will be returned.
  EXPECT_CALL(delegate_, CreateAckFrame()).WillOnce(Return(CreateAckFrame()));
  EXPECT_CALL(delegate_, CreateFeedbackFrame()).WillOnce(
      Return(CreateFeedbackFrame()));

  // Send some data and a control frame
  generator_.ConsumeData(3, "quux", 7, false);
  generator_.AddControlFrame(QuicFrame(CreateGoAwayFrame()));

  // All five frames will be flushed out in a single packet.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));
  generator_.FinishBatchOperations();
  EXPECT_FALSE(generator_.HasQueuedData());

  PacketContents contents;
  contents.num_ack_frames = 1;
  contents.num_goaway_frames = 1;
  contents.num_feedback_frames = 1;
  contents.num_rst_stream_frames = 1;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest, NotWritableThenBatchOperations2) {
  delegate_.SetCanWrite(false);

  generator_.SetShouldSendAck(true);
  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedData());

  delegate_.SetCanWrite(true);

  generator_.StartBatchOperations();

  // When the first write operation is invoked, the ack and feedback
  // frames will be returned.
  EXPECT_CALL(delegate_, CreateAckFrame()).WillOnce(Return(CreateAckFrame()));
  EXPECT_CALL(delegate_, CreateFeedbackFrame()).WillOnce(
      Return(CreateFeedbackFrame()));

  {
    InSequence dummy;
  // All five frames will be flushed out in a single packet
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet_), Return(true)));
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      DoAll(SaveArg<0>(&packet2_), Return(true)));
  }

  // Send enough data to exceed one packet
  size_t data_len = kMaxPacketSize + 100;
  QuicConsumedData consumed =
      generator_.ConsumeData(3, CreateData(data_len), 0, true);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  generator_.AddControlFrame(QuicFrame(CreateGoAwayFrame()));

  generator_.FinishBatchOperations();
  EXPECT_FALSE(generator_.HasQueuedData());

  // The first packet should have the queued data and part of the stream data.
  PacketContents contents;
  contents.num_ack_frames = 1;
  contents.num_feedback_frames = 1;
  contents.num_rst_stream_frames = 1;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, packet_);

  // The second should have the remainder of the stream data.
  PacketContents contents2;
  contents2.num_goaway_frames = 1;
  contents2.num_stream_frames = 1;
  CheckPacketContains(contents2, packet2_);
}

}  // namespace test
}  // namespace net
