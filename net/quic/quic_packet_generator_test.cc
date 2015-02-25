// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_generator.h"

#include <string>

#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_packet_creator_peer.h"
#include "net/quic/test_tools/quic_packet_generator_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/quic/test_tools/simple_quic_framer.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using std::string;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;
using testing::_;

namespace net {
namespace test {
namespace {

const int64 kMinFecTimeoutMs = 5u;

class MockDelegate : public QuicPacketGenerator::DelegateInterface {
 public:
  MockDelegate() {}
  ~MockDelegate() override {}

  MOCK_METHOD3(ShouldGeneratePacket,
               bool(TransmissionType transmission_type,
                    HasRetransmittableData retransmittable,
                    IsHandshake handshake));
  MOCK_METHOD1(PopulateAckFrame, void(QuicAckFrame*));
  MOCK_METHOD1(PopulateStopWaitingFrame, void(QuicStopWaitingFrame*));
  MOCK_METHOD1(OnSerializedPacket, void(const SerializedPacket& packet));
  MOCK_METHOD2(CloseConnection, void(QuicErrorCode, bool));

  void SetCanWriteAnything() {
    EXPECT_CALL(*this, ShouldGeneratePacket(NOT_RETRANSMISSION, _, _))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*this, ShouldGeneratePacket(NOT_RETRANSMISSION,
                                            NO_RETRANSMITTABLE_DATA, _))
        .WillRepeatedly(Return(true));
  }

  void SetCanNotWrite() {
    EXPECT_CALL(*this, ShouldGeneratePacket(NOT_RETRANSMISSION, _, _))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*this, ShouldGeneratePacket(NOT_RETRANSMISSION,
                                            NO_RETRANSMITTABLE_DATA, _))
        .WillRepeatedly(Return(false));
  }

  // Use this when only ack frames should be allowed to be written.
  void SetCanWriteOnlyNonRetransmittable() {
    EXPECT_CALL(*this, ShouldGeneratePacket(NOT_RETRANSMISSION, _, _))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*this, ShouldGeneratePacket(NOT_RETRANSMISSION,
                                            NO_RETRANSMITTABLE_DATA, _))
        .WillRepeatedly(Return(true));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

// Simple struct for describing the contents of a packet.
// Useful in conjunction with a SimpleQuicFrame for validating that a packet
// contains the expected frames.
struct PacketContents {
  PacketContents()
      : num_ack_frames(0),
        num_connection_close_frames(0),
        num_goaway_frames(0),
        num_rst_stream_frames(0),
        num_stop_waiting_frames(0),
        num_stream_frames(0),
        fec_group(0) {
  }

  size_t num_ack_frames;
  size_t num_connection_close_frames;
  size_t num_goaway_frames;
  size_t num_rst_stream_frames;
  size_t num_stop_waiting_frames;
  size_t num_stream_frames;

  QuicFecGroupNumber fec_group;
};

}  // namespace

class QuicPacketGeneratorTest : public ::testing::Test {
 protected:
  QuicPacketGeneratorTest()
      : framer_(QuicSupportedVersions(), QuicTime::Zero(), false),
        generator_(42, &framer_, &random_, &delegate_),
        creator_(QuicPacketGeneratorPeer::GetPacketCreator(&generator_)),
        packet_(0, PACKET_1BYTE_SEQUENCE_NUMBER, nullptr, 0, nullptr),
        packet2_(0, PACKET_1BYTE_SEQUENCE_NUMBER, nullptr, 0, nullptr),
        packet3_(0, PACKET_1BYTE_SEQUENCE_NUMBER, nullptr, 0, nullptr),
        packet4_(0, PACKET_1BYTE_SEQUENCE_NUMBER, nullptr, 0, nullptr),
        packet5_(0, PACKET_1BYTE_SEQUENCE_NUMBER, nullptr, 0, nullptr),
        packet6_(0, PACKET_1BYTE_SEQUENCE_NUMBER, nullptr, 0, nullptr),
        packet7_(0, PACKET_1BYTE_SEQUENCE_NUMBER, nullptr, 0, nullptr),
        packet8_(0, PACKET_1BYTE_SEQUENCE_NUMBER, nullptr, 0, nullptr) {}

  ~QuicPacketGeneratorTest() override {
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
    delete packet6_.packet;
    delete packet6_.retransmittable_frames;
    delete packet7_.packet;
    delete packet7_.retransmittable_frames;
    delete packet8_.packet;
    delete packet8_.retransmittable_frames;
  }

  QuicRstStreamFrame* CreateRstStreamFrame() {
    return new QuicRstStreamFrame(1, QUIC_STREAM_NO_ERROR, 0);
  }

  QuicGoAwayFrame* CreateGoAwayFrame() {
    return new QuicGoAwayFrame(QUIC_NO_ERROR, 1, string());
  }

  void CheckPacketContains(const PacketContents& contents,
                           const SerializedPacket& packet) {
    size_t num_retransmittable_frames = contents.num_connection_close_frames +
        contents.num_goaway_frames + contents.num_rst_stream_frames +
        contents.num_stream_frames;
    size_t num_frames = contents.num_ack_frames +
                        contents.num_stop_waiting_frames +
                        num_retransmittable_frames;

    if (num_retransmittable_frames == 0) {
      ASSERT_TRUE(packet.retransmittable_frames == nullptr);
    } else {
      ASSERT_TRUE(packet.retransmittable_frames != nullptr);
      EXPECT_EQ(num_retransmittable_frames,
                packet.retransmittable_frames->frames().size());
    }

    ASSERT_TRUE(packet.packet != nullptr);
    ASSERT_TRUE(simple_framer_.ProcessPacket(*packet.packet));
    EXPECT_EQ(num_frames, simple_framer_.num_frames());
    EXPECT_EQ(contents.num_ack_frames, simple_framer_.ack_frames().size());
    EXPECT_EQ(contents.num_connection_close_frames,
              simple_framer_.connection_close_frames().size());
    EXPECT_EQ(contents.num_goaway_frames,
              simple_framer_.goaway_frames().size());
    EXPECT_EQ(contents.num_rst_stream_frames,
              simple_framer_.rst_stream_frames().size());
    EXPECT_EQ(contents.num_stream_frames,
              simple_framer_.stream_frames().size());
    EXPECT_EQ(contents.num_stop_waiting_frames,
              simple_framer_.stop_waiting_frames().size());
    EXPECT_EQ(contents.fec_group, simple_framer_.header().fec_group);
  }

  void CheckPacketHasSingleStreamFrame(const SerializedPacket& packet) {
    ASSERT_TRUE(packet.retransmittable_frames != nullptr);
    EXPECT_EQ(1u, packet.retransmittable_frames->frames().size());
    ASSERT_TRUE(packet.packet != nullptr);
    ASSERT_TRUE(simple_framer_.ProcessPacket(*packet.packet));
    EXPECT_EQ(1u, simple_framer_.num_frames());
    EXPECT_EQ(1u, simple_framer_.stream_frames().size());
  }

  void CheckPacketIsFec(const SerializedPacket& packet,
                        QuicPacketSequenceNumber fec_group) {
    ASSERT_TRUE(packet.retransmittable_frames == nullptr);
    ASSERT_TRUE(packet.packet != nullptr);
    ASSERT_TRUE(simple_framer_.ProcessPacket(*packet.packet));
    EXPECT_TRUE(simple_framer_.header().fec_flag);
    EXPECT_EQ(fec_group, simple_framer_.fec_data().fec_group);
  }

  IOVector CreateData(size_t len) {
    data_array_.reset(new char[len]);
    memset(data_array_.get(), '?', len);
    IOVector data;
    data.Append(data_array_.get(), len);
    return data;
  }

  QuicFramer framer_;
  MockRandom random_;
  StrictMock<MockDelegate> delegate_;
  QuicPacketGenerator generator_;
  QuicPacketCreator* creator_;
  SimpleQuicFramer simple_framer_;
  SerializedPacket packet_;
  SerializedPacket packet2_;
  SerializedPacket packet3_;
  SerializedPacket packet4_;
  SerializedPacket packet5_;
  SerializedPacket packet6_;
  SerializedPacket packet7_;
  SerializedPacket packet8_;

 private:
  scoped_ptr<char[]> data_array_;
};

class MockDebugDelegate : public QuicPacketGenerator::DebugDelegate {
 public:
  MOCK_METHOD1(OnFrameAddedToPacket,
               void(const QuicFrame&));
};

TEST_F(QuicPacketGeneratorTest, ShouldSendAck_NotWritable) {
  delegate_.SetCanNotWrite();

  generator_.SetShouldSendAck(false);
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_F(QuicPacketGeneratorTest, ShouldSendAck_WritableAndShouldNotFlush) {
  StrictMock<MockDebugDelegate> debug_delegate;

  generator_.set_debug_delegate(&debug_delegate);
  delegate_.SetCanWriteOnlyNonRetransmittable();
  generator_.StartBatchOperations();

  EXPECT_CALL(delegate_, PopulateAckFrame(_));
  EXPECT_CALL(debug_delegate, OnFrameAddedToPacket(_)).Times(1);

  generator_.SetShouldSendAck(false);
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_F(QuicPacketGeneratorTest, ShouldSendAck_WritableAndShouldFlush) {
  delegate_.SetCanWriteOnlyNonRetransmittable();

  EXPECT_CALL(delegate_, PopulateAckFrame(_));
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet_));

  generator_.SetShouldSendAck(false);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_ack_frames = 1;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest, ShouldSendAck_MultipleCalls) {
  // Make sure that calling SetShouldSendAck multiple times does not result in a
  // crash. Previously this would result in multiple QuicFrames queued in the
  // packet generator, with all but the last with internal pointers to freed
  // memory.
  delegate_.SetCanWriteAnything();

  // Only one AckFrame should be created.
  EXPECT_CALL(delegate_, PopulateAckFrame(_)).Times(1);
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .Times(1)
      .WillOnce(SaveArg<0>(&packet_));

  generator_.StartBatchOperations();
  generator_.SetShouldSendAck(false);
  generator_.SetShouldSendAck(false);
  generator_.FinishBatchOperations();
}

TEST_F(QuicPacketGeneratorTest, AddControlFrame_NotWritable) {
  delegate_.SetCanNotWrite();

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_F(QuicPacketGeneratorTest, AddControlFrame_OnlyAckWritable) {
  delegate_.SetCanWriteOnlyNonRetransmittable();

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_F(QuicPacketGeneratorTest, AddControlFrame_WritableAndShouldNotFlush) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_F(QuicPacketGeneratorTest, AddControlFrame_NotWritableBatchThenFlush) {
  delegate_.SetCanNotWrite();
  generator_.StartBatchOperations();

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedFrames());
  generator_.FinishBatchOperations();
  EXPECT_TRUE(generator_.HasQueuedFrames());

  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet_));
  generator_.FlushAllQueuedFrames();
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_rst_stream_frames = 1;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest, AddControlFrame_WritableAndShouldFlush) {
  delegate_.SetCanWriteAnything();

  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet_));

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_rst_stream_frames = 1;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest, ConsumeData_NotWritable) {
  delegate_.SetCanNotWrite();

  QuicConsumedData consumed = generator_.ConsumeData(
      kHeadersStreamId, MakeIOVector("foo"), 2, true, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(0u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());
}

TEST_F(QuicPacketGeneratorTest, ConsumeData_WritableAndShouldNotFlush) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  QuicConsumedData consumed = generator_.ConsumeData(
      kHeadersStreamId, MakeIOVector("foo"), 2, true, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_F(QuicPacketGeneratorTest, ConsumeData_WritableAndShouldFlush) {
  delegate_.SetCanWriteAnything();

  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet_));
  QuicConsumedData consumed = generator_.ConsumeData(
      kHeadersStreamId, MakeIOVector("foo"), 2, true, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest, ConsumeData_EmptyData) {
  EXPECT_DFATAL(generator_.ConsumeData(kHeadersStreamId, MakeIOVector(""), 0,
                                       false, MAY_FEC_PROTECT, nullptr),
                "Attempt to consume empty data without FIN.");
}

TEST_F(QuicPacketGeneratorTest,
       ConsumeDataMultipleTimes_WritableAndShouldNotFlush) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  generator_.ConsumeData(kHeadersStreamId, MakeIOVector("foo"), 2, true,
                         MAY_FEC_PROTECT, nullptr);
  QuicConsumedData consumed = generator_.ConsumeData(
      3, MakeIOVector("quux"), 7, false, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(4u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_F(QuicPacketGeneratorTest, ConsumeData_BatchOperations) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  generator_.ConsumeData(kHeadersStreamId, MakeIOVector("foo"), 2, true,
                         MAY_FEC_PROTECT, nullptr);
  QuicConsumedData consumed = generator_.ConsumeData(
      3, MakeIOVector("quux"), 7, false, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(4u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedFrames());

  // Now both frames will be flushed out.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet_));
  generator_.FinishBatchOperations();
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_stream_frames = 2;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest, ConsumeDataSendsFecOnMaxGroupSize) {
  delegate_.SetCanWriteAnything();

  // Send FEC every two packets.
  creator_->set_max_packets_per_fec_group(2);

  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet_));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet2_));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet3_));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet4_));
  }

  // Send enough data to create 3 packets: two full and one partial. Send with
  // MUST_FEC_PROTECT flag.
  size_t data_len = 2 * kDefaultMaxPacketSize + 100;
  QuicConsumedData consumed = generator_.ConsumeData(
      3, CreateData(data_len), 0, true, MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  CheckPacketHasSingleStreamFrame(packet_);
  CheckPacketHasSingleStreamFrame(packet2_);
  CheckPacketIsFec(packet3_, 1);
  CheckPacketHasSingleStreamFrame(packet4_);
  EXPECT_TRUE(creator_->IsFecProtected());

  // The FEC packet under construction will be sent when one more packet is sent
  // (since FEC group size is 2), or when OnFecTimeout is called. Send more data
  // with MAY_FEC_PROTECT. This packet should also be protected, and FEC packet
  // is sent since FEC group size is reached.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(SaveArg<0>(&packet5_));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(SaveArg<0>(&packet6_));
  }
  consumed = generator_.ConsumeData(5, CreateData(1u), 0, true, MAY_FEC_PROTECT,
                                    nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  CheckPacketHasSingleStreamFrame(packet5_);
  CheckPacketIsFec(packet6_, 4);
  EXPECT_FALSE(creator_->IsFecProtected());
}

TEST_F(QuicPacketGeneratorTest, ConsumeDataSendsFecOnTimeout) {
  delegate_.SetCanWriteAnything();
  creator_->set_max_packets_per_fec_group(1000);

  // Send data with MUST_FEC_PROTECT flag. No FEC packet is emitted, but the
  // creator FEC protects all data.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet_));
  QuicConsumedData consumed = generator_.ConsumeData(3, CreateData(1u), 0, true,
                                                     MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  CheckPacketHasSingleStreamFrame(packet_);
  EXPECT_TRUE(creator_->IsFecProtected());

  // Send more data with MAY_FEC_PROTECT. This packet should also be protected,
  // and FEC packet is not yet sent.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet2_));
  consumed = generator_.ConsumeData(5, CreateData(1u), 0, true, MAY_FEC_PROTECT,
                                    nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  CheckPacketHasSingleStreamFrame(packet2_);
  EXPECT_TRUE(creator_->IsFecProtected());

  // Calling OnFecTimeout should cause the FEC packet to be emitted.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet3_));
  generator_.OnFecTimeout();
  CheckPacketIsFec(packet3_, 1);
  EXPECT_FALSE(creator_->IsFecProtected());

  // Subsequent data is protected under the next FEC group. Send enough data to
  // create 2 more packets: one full and one partial.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(SaveArg<0>(&packet4_));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(SaveArg<0>(&packet5_));
  }
  size_t data_len = kDefaultMaxPacketSize + 1;
  consumed = generator_.ConsumeData(7, CreateData(data_len), 0, true,
                                    MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  CheckPacketHasSingleStreamFrame(packet4_);
  CheckPacketHasSingleStreamFrame(packet5_);
  EXPECT_TRUE(creator_->IsFecProtected());

  // Calling OnFecTimeout should cause the FEC packet to be emitted.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet6_));
  generator_.OnFecTimeout();
  CheckPacketIsFec(packet6_, 4);
  EXPECT_FALSE(creator_->IsFecProtected());
}

TEST_F(QuicPacketGeneratorTest, GetFecTimeoutFiniteOnlyOnFirstPacketInGroup) {
  delegate_.SetCanWriteAnything();
  creator_->set_max_packets_per_fec_group(6);

  // Send enough data to create 2 packets: one full and one partial. Send with
  // MUST_FEC_PROTECT flag. No FEC packet is emitted yet, but the creator FEC
  // protects all data.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet_));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet2_));
  }
  size_t data_len = 1 * kDefaultMaxPacketSize + 100;
  QuicConsumedData consumed = generator_.ConsumeData(
      3, CreateData(data_len), 0, true, MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());
  CheckPacketHasSingleStreamFrame(packet_);
  CheckPacketHasSingleStreamFrame(packet2_);
  EXPECT_TRUE(creator_->IsFecProtected());

  // GetFecTimeout returns finite timeout only for first packet in group.
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(kMinFecTimeoutMs),
            generator_.GetFecTimeout(/*sequence_number=*/1u));
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            generator_.GetFecTimeout(/*sequence_number=*/2u));

  // Send more data with MAY_FEC_PROTECT. This packet should also be protected,
  // and FEC packet is not yet sent.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet3_));
  consumed = generator_.ConsumeData(5, CreateData(1u), 0, true, MAY_FEC_PROTECT,
                                    nullptr);
  CheckPacketHasSingleStreamFrame(packet3_);
  EXPECT_TRUE(creator_->IsFecProtected());

  // GetFecTimeout returns finite timeout only for first packet in group.
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            generator_.GetFecTimeout(/*sequence_number=*/3u));

  // Calling OnFecTimeout should cause the FEC packet to be emitted.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet4_));
  generator_.OnFecTimeout();
  CheckPacketIsFec(packet4_, /*fec_group=*/1u);
  EXPECT_FALSE(creator_->IsFecProtected());

  // Subsequent data is protected under the next FEC group. Send enough data to
  // create 2 more packets: one full and one partial.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(SaveArg<0>(&packet5_));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(SaveArg<0>(&packet6_));
  }
  data_len = kDefaultMaxPacketSize + 1u;
  consumed = generator_.ConsumeData(7, CreateData(data_len), 0, true,
                                    MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  CheckPacketHasSingleStreamFrame(packet5_);
  CheckPacketHasSingleStreamFrame(packet6_);
  EXPECT_TRUE(creator_->IsFecProtected());

  // GetFecTimeout returns finite timeout for first packet in the new group.
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(kMinFecTimeoutMs),
            generator_.GetFecTimeout(/*sequence_number=*/5u));
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            generator_.GetFecTimeout(/*sequence_number=*/6u));

  // Calling OnFecTimeout should cause the FEC packet to be emitted.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet7_));
  generator_.OnFecTimeout();
  CheckPacketIsFec(packet7_, /*fec_group=*/5u);
  EXPECT_FALSE(creator_->IsFecProtected());

  // Send more data with MAY_FEC_PROTECT. No FEC protection, so GetFecTimeout
  // returns infinite.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet8_));
  consumed = generator_.ConsumeData(9, CreateData(1u), 0, true, MAY_FEC_PROTECT,
                                    nullptr);
  CheckPacketHasSingleStreamFrame(packet8_);
  EXPECT_FALSE(creator_->IsFecProtected());
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            generator_.GetFecTimeout(/*sequence_number=*/8u));
}

TEST_F(QuicPacketGeneratorTest, ConsumeData_FramesPreviouslyQueued) {
  // Set the packet size be enough for two stream frames with 0 stream offset,
  // but not enough for a stream frame of 0 offset and one with non-zero offset.
  size_t length =
      NullEncrypter().GetCiphertextSize(0) +
      GetPacketHeaderSize(
          creator_->connection_id_length(), true,
          QuicPacketCreatorPeer::NextSequenceNumberLength(creator_),
          NOT_IN_FEC_GROUP) +
      // Add an extra 3 bytes for the payload and 1 byte so BytesFree is larger
      // than the GetMinStreamFrameSize.
      QuicFramer::GetMinStreamFrameSize(1, 0, false, NOT_IN_FEC_GROUP) + 3 +
      QuicFramer::GetMinStreamFrameSize(1, 0, true, NOT_IN_FEC_GROUP) + 1;
  creator_->set_max_packet_length(length);
  delegate_.SetCanWriteAnything();
  {
     InSequence dummy;
     EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
         SaveArg<0>(&packet_));
     EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
         SaveArg<0>(&packet2_));
  }
  generator_.StartBatchOperations();
  // Queue enough data to prevent a stream frame with a non-zero offset from
  // fitting.
  QuicConsumedData consumed =
      generator_.ConsumeData(kHeadersStreamId, MakeIOVector("foo"), 0, false,
                             MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedFrames());

  // This frame will not fit with the existing frame, causing the queued frame
  // to be serialized, and it will not fit with another frame like it, so it is
  // serialized by itself.
  consumed = generator_.ConsumeData(kHeadersStreamId, MakeIOVector("bar"), 3,
                                    true, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, packet_);
  CheckPacketContains(contents, packet2_);
}

TEST_F(QuicPacketGeneratorTest, NoFecPacketSentWhenBatchEnds) {
  delegate_.SetCanWriteAnything();
  creator_->set_max_packets_per_fec_group(6);

  generator_.StartBatchOperations();

  generator_.ConsumeData(3, MakeIOVector("foo"), 2, true, MUST_FEC_PROTECT,
                         nullptr);
  QuicConsumedData consumed = generator_.ConsumeData(
      5, MakeIOVector("quux"), 7, false, MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(4u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedFrames());

  // Now both frames will be flushed out, but FEC packet is not yet sent.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet_));
  generator_.FinishBatchOperations();
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_stream_frames = 2u;
  contents.fec_group = 1u;
  CheckPacketContains(contents, packet_);

  // Forcing FEC timeout causes FEC packet to be emitted.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet2_));
  generator_.OnFecTimeout();
  CheckPacketIsFec(packet2_, /*fec_group=*/1u);
}

TEST_F(QuicPacketGeneratorTest, FecTimeoutOnRttChange) {
  EXPECT_EQ(QuicTime::Delta::Zero(),
            QuicPacketGeneratorPeer::GetFecTimeout(&generator_));
  generator_.OnRttChange(QuicTime::Delta::FromMilliseconds(300));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(150),
            QuicPacketGeneratorPeer::GetFecTimeout(&generator_));
}

TEST_F(QuicPacketGeneratorTest, FecGroupSizeOnCongestionWindowChange) {
  delegate_.SetCanWriteAnything();
  creator_->set_max_packets_per_fec_group(50);
  EXPECT_EQ(50u, creator_->max_packets_per_fec_group());
  EXPECT_FALSE(creator_->IsFecGroupOpen());

  // On reduced cwnd.
  generator_.OnCongestionWindowChange(7);
  EXPECT_EQ(3u, creator_->max_packets_per_fec_group());

  // On increased cwnd.
  generator_.OnCongestionWindowChange(100);
  EXPECT_EQ(50u, creator_->max_packets_per_fec_group());

  // On collapsed cwnd.
  generator_.OnCongestionWindowChange(1);
  EXPECT_EQ(2u, creator_->max_packets_per_fec_group());
}

TEST_F(QuicPacketGeneratorTest, FecGroupSizeChangeWithOpenGroup) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();
  creator_->set_max_packets_per_fec_group(50);
  EXPECT_EQ(50u, creator_->max_packets_per_fec_group());
  EXPECT_FALSE(creator_->IsFecGroupOpen());

  // Send enough data to create 4 packets with MUST_FEC_PROTECT flag. 3 packets
  // are sent, one is queued in the creator.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet_));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet2_));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet3_));
  }
  size_t data_len = 3 * kDefaultMaxPacketSize + 1;
  QuicConsumedData consumed = generator_.ConsumeData(
      7, CreateData(data_len), 0, true, MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(creator_->IsFecGroupOpen());

  // Change FEC groupsize.
  generator_.OnCongestionWindowChange(2);
  EXPECT_EQ(2u, creator_->max_packets_per_fec_group());

  // Send enough data to trigger one unprotected data packet, causing the FEC
  // packet to also be sent.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet4_));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet5_));
  }
  consumed = generator_.ConsumeData(7, CreateData(kDefaultMaxPacketSize), 0,
                                    true, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(kDefaultMaxPacketSize, consumed.bytes_consumed);
  // Verify that one FEC packet was sent.
  CheckPacketIsFec(packet5_, /*fec_group=*/1u);
  EXPECT_FALSE(creator_->IsFecGroupOpen());
  EXPECT_FALSE(creator_->IsFecProtected());
}

TEST_F(QuicPacketGeneratorTest, SwitchFecOnOff) {
  delegate_.SetCanWriteAnything();
  creator_->set_max_packets_per_fec_group(2);
  EXPECT_FALSE(creator_->IsFecProtected());

  // Send one unprotected data packet.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet_));
  QuicConsumedData consumed = generator_.ConsumeData(5, CreateData(1u), 0, true,
                                                     MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());
  EXPECT_FALSE(creator_->IsFecProtected());
  // Verify that one data packet was sent.
  PacketContents contents;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, packet_);

  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet2_));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet3_));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet4_));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet5_));
  }
  // Send enough data to create 3 packets with MUST_FEC_PROTECT flag.
  size_t data_len = 2 * kDefaultMaxPacketSize + 100;
  consumed = generator_.ConsumeData(7, CreateData(data_len), 0, true,
                                    MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  // Verify that packets sent were 3 data and 1 FEC.
  CheckPacketHasSingleStreamFrame(packet2_);
  CheckPacketHasSingleStreamFrame(packet3_);
  CheckPacketIsFec(packet4_, /*fec_group=*/2u);
  CheckPacketHasSingleStreamFrame(packet5_);

  // Calling OnFecTimeout should emit the pending FEC packet.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet6_));
  generator_.OnFecTimeout();
  CheckPacketIsFec(packet6_, /*fec_group=*/5u);

  // Send one unprotected data packet.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet7_));
  consumed = generator_.ConsumeData(7, CreateData(1u), 0, true, MAY_FEC_PROTECT,
                                    nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());
  EXPECT_FALSE(creator_->IsFecProtected());
  // Verify that one unprotected data packet was sent.
  CheckPacketContains(contents, packet7_);
}

TEST_F(QuicPacketGeneratorTest, SwitchFecOnWithPendingFrameInCreator) {
  delegate_.SetCanWriteAnything();
  // Enable FEC.
  creator_->set_max_packets_per_fec_group(2);

  generator_.StartBatchOperations();
  // Queue enough data to prevent a stream frame with a non-zero offset from
  // fitting.
  QuicConsumedData consumed = generator_.ConsumeData(7, CreateData(1u), 0, true,
                                                     MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  EXPECT_TRUE(creator_->HasPendingFrames());

  // Queue protected data for sending. Should cause queued frames to be flushed.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      SaveArg<0>(&packet_));
  EXPECT_FALSE(creator_->IsFecProtected());
  consumed = generator_.ConsumeData(7, CreateData(1u), 0, true,
                                    MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  PacketContents contents;
  contents.num_stream_frames = 1;
  // Transmitted packet was not FEC protected.
  CheckPacketContains(contents, packet_);
  EXPECT_TRUE(creator_->IsFecProtected());
  EXPECT_TRUE(creator_->HasPendingFrames());
}

TEST_F(QuicPacketGeneratorTest, SwitchFecOnWithPendingFramesInGenerator) {
  // Enable FEC.
  creator_->set_max_packets_per_fec_group(2);

  // Queue control frames in generator.
  delegate_.SetCanNotWrite();
  generator_.SetShouldSendAck(true);
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  // Set up frames to write into the creator when control frames are written.
  EXPECT_CALL(delegate_, PopulateAckFrame(_));
  EXPECT_CALL(delegate_, PopulateStopWaitingFrame(_));

  // Generator should have queued control frames, and creator should be empty.
  EXPECT_TRUE(generator_.HasQueuedFrames());
  EXPECT_FALSE(creator_->HasPendingFrames());
  EXPECT_FALSE(creator_->IsFecProtected());

  // Queue protected data for sending. Should cause queued frames to be flushed.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      SaveArg<0>(&packet_));
  QuicConsumedData consumed = generator_.ConsumeData(7, CreateData(1u), 0, true,
                                                     MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  PacketContents contents;
  contents.num_ack_frames = 1;
  contents.num_stop_waiting_frames = 1;
  CheckPacketContains(contents, packet_);

  // FEC protection should be on in creator.
  EXPECT_TRUE(creator_->IsFecProtected());
}

TEST_F(QuicPacketGeneratorTest, SwitchFecOnOffWithSubsequentFramesProtected) {
  delegate_.SetCanWriteAnything();

  // Enable FEC.
  creator_->set_max_packets_per_fec_group(2);
  EXPECT_FALSE(creator_->IsFecProtected());

  // Queue stream frame to be protected in creator.
  generator_.StartBatchOperations();
  QuicConsumedData consumed = generator_.ConsumeData(5, CreateData(1u), 0, true,
                                                     MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  // Creator has a pending protected frame.
  EXPECT_TRUE(creator_->HasPendingFrames());
  EXPECT_TRUE(creator_->IsFecProtected());

  // Add enough unprotected data to exceed size of current packet, so that
  // current packet is sent. Both frames will be sent out in a single packet.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet_));
  size_t data_len = kDefaultMaxPacketSize;
  consumed = generator_.ConsumeData(5, CreateData(data_len), 0, true,
                                    MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  PacketContents contents;
  contents.num_stream_frames = 2u;
  contents.fec_group = 1u;
  CheckPacketContains(contents, packet_);
  // FEC protection should still be on in creator.
  EXPECT_TRUE(creator_->IsFecProtected());
}

TEST_F(QuicPacketGeneratorTest, SwitchFecOnOffWithSubsequentPacketsProtected) {
  delegate_.SetCanWriteAnything();

  // Enable FEC.
  creator_->set_max_packets_per_fec_group(2);
  EXPECT_FALSE(creator_->IsFecProtected());

  // Send first packet, FEC protected.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet_));
  QuicConsumedData consumed = generator_.ConsumeData(5, CreateData(1u), 0, true,
                                                     MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  PacketContents contents;
  contents.num_stream_frames = 1u;
  contents.fec_group = 1u;
  CheckPacketContains(contents, packet_);

  // FEC should still be on in creator.
  EXPECT_TRUE(creator_->IsFecProtected());

  // Send unprotected data to cause second packet to be sent, which gets
  // protected because it happens to fall within an open FEC group. Data packet
  // will be followed by FEC packet.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet2_));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet3_));
  }
  consumed = generator_.ConsumeData(5, CreateData(1u), 0, true, MAY_FEC_PROTECT,
                                    nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  contents.num_stream_frames = 1u;
  CheckPacketContains(contents, packet2_);
  CheckPacketIsFec(packet3_, /*fec_group=*/1u);

  // FEC protection should be off in creator.
  EXPECT_FALSE(creator_->IsFecProtected());
}

TEST_F(QuicPacketGeneratorTest, SwitchFecOnOffThenOnWithCreatorProtectionOn) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  // Enable FEC.
  creator_->set_max_packets_per_fec_group(2);
  EXPECT_FALSE(creator_->IsFecProtected());

  // Queue one byte of FEC protected data.
  QuicConsumedData consumed = generator_.ConsumeData(5, CreateData(1u), 0, true,
                                                     MUST_FEC_PROTECT, nullptr);
  EXPECT_TRUE(creator_->HasPendingFrames());

  // Add more unprotected data causing first packet to be sent, FEC protected.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
      SaveArg<0>(&packet_));
  size_t data_len = kDefaultMaxPacketSize;
  consumed = generator_.ConsumeData(5, CreateData(data_len), 0, true,
                                    MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  PacketContents contents;
  contents.num_stream_frames = 2u;
  contents.fec_group = 1u;
  CheckPacketContains(contents, packet_);

  // FEC group is still open in creator.
  EXPECT_TRUE(creator_->IsFecProtected());

  // Add data that should be protected, large enough to cause second packet to
  // be sent. Data packet should be followed by FEC packet.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet2_));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet3_));
  }
  consumed = generator_.ConsumeData(5, CreateData(data_len), 0, true,
                                    MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  CheckPacketContains(contents, packet2_);
  CheckPacketIsFec(packet3_, /*fec_group=*/1u);

  // FEC protection should remain on in creator.
  EXPECT_TRUE(creator_->IsFecProtected());
}

TEST_F(QuicPacketGeneratorTest, NotWritableThenBatchOperations) {
  delegate_.SetCanNotWrite();

  generator_.SetShouldSendAck(false);
  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedFrames());

  delegate_.SetCanWriteAnything();

  generator_.StartBatchOperations();

  // When the first write operation is invoked, the ack frame will be returned.
  EXPECT_CALL(delegate_, PopulateAckFrame(_));

  // Send some data and a control frame
  generator_.ConsumeData(3, MakeIOVector("quux"), 7, false, MAY_FEC_PROTECT,
                         nullptr);
  generator_.AddControlFrame(QuicFrame(CreateGoAwayFrame()));

  // All five frames will be flushed out in a single packet.
  EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(SaveArg<0>(&packet_));
  generator_.FinishBatchOperations();
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_ack_frames = 1;
  contents.num_goaway_frames = 1;
  contents.num_rst_stream_frames = 1;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, packet_);
}

TEST_F(QuicPacketGeneratorTest, NotWritableThenBatchOperations2) {
  delegate_.SetCanNotWrite();

  generator_.SetShouldSendAck(false);
  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedFrames());

  delegate_.SetCanWriteAnything();

  generator_.StartBatchOperations();

  // When the first write operation is invoked, the ack frame will be returned.
  EXPECT_CALL(delegate_, PopulateAckFrame(_));

  {
    InSequence dummy;
    // All five frames will be flushed out in a single packet
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet_));
    EXPECT_CALL(delegate_, OnSerializedPacket(_)).WillOnce(
        SaveArg<0>(&packet2_));
  }

  // Send enough data to exceed one packet
  size_t data_len = kDefaultMaxPacketSize + 100;
  QuicConsumedData consumed = generator_.ConsumeData(
      3, CreateData(data_len), 0, true, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  generator_.AddControlFrame(QuicFrame(CreateGoAwayFrame()));

  generator_.FinishBatchOperations();
  EXPECT_FALSE(generator_.HasQueuedFrames());

  // The first packet should have the queued data and part of the stream data.
  PacketContents contents;
  contents.num_ack_frames = 1;
  contents.num_rst_stream_frames = 1;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, packet_);

  // The second should have the remainder of the stream data.
  PacketContents contents2;
  contents2.num_goaway_frames = 1;
  contents2.num_stream_frames = 1;
  CheckPacketContains(contents2, packet2_);
}

TEST_F(QuicPacketGeneratorTest, TestConnectionIdLength) {
  generator_.SetConnectionIdLength(0);
  EXPECT_EQ(PACKET_0BYTE_CONNECTION_ID, creator_->connection_id_length());
  generator_.SetConnectionIdLength(1);
  EXPECT_EQ(PACKET_1BYTE_CONNECTION_ID, creator_->connection_id_length());
  generator_.SetConnectionIdLength(2);
  EXPECT_EQ(PACKET_4BYTE_CONNECTION_ID, creator_->connection_id_length());
  generator_.SetConnectionIdLength(3);
  EXPECT_EQ(PACKET_4BYTE_CONNECTION_ID, creator_->connection_id_length());
  generator_.SetConnectionIdLength(4);
  EXPECT_EQ(PACKET_4BYTE_CONNECTION_ID, creator_->connection_id_length());
  generator_.SetConnectionIdLength(5);
  EXPECT_EQ(PACKET_8BYTE_CONNECTION_ID, creator_->connection_id_length());
  generator_.SetConnectionIdLength(6);
  EXPECT_EQ(PACKET_8BYTE_CONNECTION_ID, creator_->connection_id_length());
  generator_.SetConnectionIdLength(7);
  EXPECT_EQ(PACKET_8BYTE_CONNECTION_ID, creator_->connection_id_length());
  generator_.SetConnectionIdLength(8);
  EXPECT_EQ(PACKET_8BYTE_CONNECTION_ID, creator_->connection_id_length());
  generator_.SetConnectionIdLength(9);
  EXPECT_EQ(PACKET_8BYTE_CONNECTION_ID, creator_->connection_id_length());
}

}  // namespace test
}  // namespace net
