// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_generator.h"

#include <string>

#include "base/macros.h"
#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_flags.h"
#include "net/quic/quic_simple_buffer_allocator.h"
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
using std::vector;
using testing::InSequence;
using testing::Return;
using testing::StrictMock;
using testing::_;

namespace net {
namespace test {
namespace {

const int64_t kMinFecTimeoutMs = 5u;

static const FecSendPolicy kFecSendPolicyList[] = {
    FEC_ANY_TRIGGER, FEC_ALARM_TRIGGER,
};

class MockDelegate : public QuicPacketGenerator::DelegateInterface {
 public:
  MockDelegate() {}
  ~MockDelegate() override {}

  MOCK_METHOD2(ShouldGeneratePacket,
               bool(HasRetransmittableData retransmittable,
                    IsHandshake handshake));
  MOCK_METHOD1(PopulateAckFrame, void(QuicAckFrame*));
  MOCK_METHOD1(PopulateStopWaitingFrame, void(QuicStopWaitingFrame*));
  MOCK_METHOD1(OnSerializedPacket, void(SerializedPacket* packet));
  MOCK_METHOD2(CloseConnection, void(QuicErrorCode, bool));
  MOCK_METHOD0(OnResetFecGroup, void());

  void SetCanWriteAnything() {
    EXPECT_CALL(*this, ShouldGeneratePacket(_, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*this, ShouldGeneratePacket(NO_RETRANSMITTABLE_DATA, _))
        .WillRepeatedly(Return(true));
  }

  void SetCanNotWrite() {
    EXPECT_CALL(*this, ShouldGeneratePacket(_, _))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*this, ShouldGeneratePacket(NO_RETRANSMITTABLE_DATA, _))
        .WillRepeatedly(Return(false));
  }

  // Use this when only ack frames should be allowed to be written.
  void SetCanWriteOnlyNonRetransmittable() {
    EXPECT_CALL(*this, ShouldGeneratePacket(_, _))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*this, ShouldGeneratePacket(NO_RETRANSMITTABLE_DATA, _))
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
        num_ping_frames(0),
        num_mtu_discovery_frames(0),
        fec_group(0) {}

  size_t num_ack_frames;
  size_t num_connection_close_frames;
  size_t num_goaway_frames;
  size_t num_rst_stream_frames;
  size_t num_stop_waiting_frames;
  size_t num_stream_frames;
  size_t num_ping_frames;
  size_t num_mtu_discovery_frames;

  QuicFecGroupNumber fec_group;
};

}  // namespace

class QuicPacketGeneratorTest : public ::testing::TestWithParam<FecSendPolicy> {
 public:
  QuicPacketGeneratorTest()
      : framer_(QuicSupportedVersions(),
                QuicTime::Zero(),
                Perspective::IS_CLIENT),
        generator_(42, &framer_, &random_, &buffer_allocator_, &delegate_),
        creator_(QuicPacketGeneratorPeer::GetPacketCreator(&generator_)) {
    generator_.set_fec_send_policy(GetParam());
    // TODO(ianswett): Fix this test so it uses a non-null encrypter.
    FLAGS_quic_never_write_unencrypted_data = false;
    FLAGS_quic_no_unencrypted_fec = false;
  }

  ~QuicPacketGeneratorTest() override {
    for (SerializedPacket& packet : packets_) {
      QuicUtils::ClearSerializedPacket(&packet);
    }
  }

  void SavePacket(SerializedPacket* packet) {
    packets_.push_back(*packet);
    ASSERT_FALSE(packet->packet->owns_buffer());
    scoped_ptr<QuicEncryptedPacket> encrypted_deleter(packets_.back().packet);
    packets_.back().packet = packets_.back().packet->Clone();
  }

 protected:
  QuicRstStreamFrame* CreateRstStreamFrame() {
    return new QuicRstStreamFrame(1, QUIC_STREAM_NO_ERROR, 0);
  }

  QuicGoAwayFrame* CreateGoAwayFrame() {
    return new QuicGoAwayFrame(QUIC_NO_ERROR, 1, string());
  }

  void CheckPacketContains(const PacketContents& contents,
                           size_t packet_index) {
    ASSERT_GT(packets_.size(), packet_index);
    const SerializedPacket& packet = packets_[packet_index];
    size_t num_retransmittable_frames =
        contents.num_connection_close_frames + contents.num_goaway_frames +
        contents.num_rst_stream_frames + contents.num_stream_frames +
        contents.num_ping_frames;
    size_t num_frames =
        contents.num_ack_frames + contents.num_stop_waiting_frames +
        contents.num_mtu_discovery_frames + num_retransmittable_frames;

    if (num_retransmittable_frames == 0) {
      ASSERT_TRUE(packet.retransmittable_frames == nullptr);
    } else {
      ASSERT_TRUE(packet.retransmittable_frames != nullptr);
      EXPECT_EQ(num_retransmittable_frames,
                packet.retransmittable_frames->size());
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

    // From the receiver's perspective, MTU discovery frames are ping frames.
    EXPECT_EQ(contents.num_ping_frames + contents.num_mtu_discovery_frames,
              simple_framer_.ping_frames().size());
  }

  void CheckPacketHasSingleStreamFrame(size_t packet_index) {
    ASSERT_GT(packets_.size(), packet_index);
    const SerializedPacket& packet = packets_[packet_index];
    ASSERT_TRUE(packet.retransmittable_frames != nullptr);
    EXPECT_EQ(1u, packet.retransmittable_frames->size());
    ASSERT_TRUE(packet.packet != nullptr);
    ASSERT_TRUE(simple_framer_.ProcessPacket(*packet.packet));
    EXPECT_EQ(1u, simple_framer_.num_frames());
    EXPECT_EQ(1u, simple_framer_.stream_frames().size());
  }

  void CheckAllPacketsHaveSingleStreamFrame() {
    for (size_t i = 0; i < packets_.size(); i++) {
      CheckPacketHasSingleStreamFrame(i);
    }
  }

  void CheckPacketIsFec(size_t packet_index, QuicPacketNumber fec_group) {
    ASSERT_GT(packets_.size(), packet_index);
    const SerializedPacket& packet = packets_[packet_index];
    ASSERT_TRUE(packet.retransmittable_frames == nullptr);
    ASSERT_TRUE(packet.packet != nullptr);
    ASSERT_TRUE(simple_framer_.ProcessPacket(*packet.packet));
    EXPECT_TRUE(simple_framer_.header().fec_flag);
  }

  QuicIOVector CreateData(size_t len) {
    data_array_.reset(new char[len]);
    memset(data_array_.get(), '?', len);
    iov_.iov_base = data_array_.get();
    iov_.iov_len = len;
    return QuicIOVector(&iov_, 1, len);
  }

  QuicIOVector MakeIOVector(StringPiece s) {
    return ::net::MakeIOVector(s, &iov_);
  }

  QuicFramer framer_;
  MockRandom random_;
  SimpleBufferAllocator buffer_allocator_;
  StrictMock<MockDelegate> delegate_;
  QuicPacketGenerator generator_;
  QuicPacketCreator* creator_;
  SimpleQuicFramer simple_framer_;
  vector<SerializedPacket> packets_;

 private:
  scoped_ptr<char[]> data_array_;
  struct iovec iov_;
};

class MockDebugDelegate : public QuicPacketCreator::DebugDelegate {
 public:
  MOCK_METHOD1(OnFrameAddedToPacket, void(const QuicFrame&));
};

// Run all end to end tests with all supported FEC send polocies.
INSTANTIATE_TEST_CASE_P(FecSendPolicy,
                        QuicPacketGeneratorTest,
                        ::testing::ValuesIn(kFecSendPolicyList));

TEST_P(QuicPacketGeneratorTest, ShouldSendAck_NotWritable) {
  delegate_.SetCanNotWrite();

  generator_.SetShouldSendAck(false);
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_P(QuicPacketGeneratorTest, ShouldSendAck_WritableAndShouldNotFlush) {
  StrictMock<MockDebugDelegate> debug_delegate;

  generator_.set_debug_delegate(&debug_delegate);
  delegate_.SetCanWriteOnlyNonRetransmittable();
  generator_.StartBatchOperations();

  EXPECT_CALL(delegate_, PopulateAckFrame(_));
  EXPECT_CALL(debug_delegate, OnFrameAddedToPacket(_)).Times(1);

  generator_.SetShouldSendAck(false);
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_P(QuicPacketGeneratorTest, ShouldSendAck_WritableAndShouldFlush) {
  delegate_.SetCanWriteOnlyNonRetransmittable();

  EXPECT_CALL(delegate_, PopulateAckFrame(_));
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));

  generator_.SetShouldSendAck(false);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_ack_frames = 1;
  CheckPacketContains(contents, 0);
}

TEST_P(QuicPacketGeneratorTest, ShouldSendAck_MultipleCalls) {
  // Make sure that calling SetShouldSendAck multiple times does not result in a
  // crash. Previously this would result in multiple QuicFrames queued in the
  // packet generator, with all but the last with internal pointers to freed
  // memory.
  delegate_.SetCanWriteAnything();

  // Only one AckFrame should be created.
  EXPECT_CALL(delegate_, PopulateAckFrame(_)).Times(1);
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .Times(1)
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));

  generator_.StartBatchOperations();
  generator_.SetShouldSendAck(false);
  generator_.SetShouldSendAck(false);
  generator_.FinishBatchOperations();
}

TEST_P(QuicPacketGeneratorTest, AddControlFrame_NotWritable) {
  delegate_.SetCanNotWrite();

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_P(QuicPacketGeneratorTest, AddControlFrame_OnlyAckWritable) {
  delegate_.SetCanWriteOnlyNonRetransmittable();

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_P(QuicPacketGeneratorTest, AddControlFrame_WritableAndShouldNotFlush) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_P(QuicPacketGeneratorTest, AddControlFrame_NotWritableBatchThenFlush) {
  delegate_.SetCanNotWrite();
  generator_.StartBatchOperations();

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_TRUE(generator_.HasQueuedFrames());
  generator_.FinishBatchOperations();
  EXPECT_TRUE(generator_.HasQueuedFrames());

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  generator_.FlushAllQueuedFrames();
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_rst_stream_frames = 1;
  CheckPacketContains(contents, 0);
}

TEST_P(QuicPacketGeneratorTest, AddControlFrame_WritableAndShouldFlush) {
  delegate_.SetCanWriteAnything();

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));

  generator_.AddControlFrame(QuicFrame(CreateRstStreamFrame()));
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_rst_stream_frames = 1;
  CheckPacketContains(contents, 0);
}

TEST_P(QuicPacketGeneratorTest, ConsumeData_NotWritable) {
  delegate_.SetCanNotWrite();

  QuicConsumedData consumed = generator_.ConsumeData(
      kHeadersStreamId, MakeIOVector("foo"), 2, true, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(0u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());
}

TEST_P(QuicPacketGeneratorTest, ConsumeData_WritableAndShouldNotFlush) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  QuicConsumedData consumed = generator_.ConsumeData(
      kHeadersStreamId, MakeIOVector("foo"), 2, true, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedFrames());
}

TEST_P(QuicPacketGeneratorTest, ConsumeData_WritableAndShouldFlush) {
  delegate_.SetCanWriteAnything();

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  QuicConsumedData consumed = generator_.ConsumeData(
      kHeadersStreamId, MakeIOVector("foo"), 2, true, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, 0);
}

// Test the behavior of ConsumeData when the data consumed is for the crypto
// handshake stream.  Ensure that the packet is always sent and padded even if
// the generator operates in batch mode.
TEST_P(QuicPacketGeneratorTest, ConsumeData_Handshake) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  QuicConsumedData consumed = generator_.ConsumeData(
      kCryptoStreamId, MakeIOVector("foo"), 0, false, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, 0);

  ASSERT_EQ(1u, packets_.size());
  ASSERT_EQ(kDefaultMaxPacketSize, generator_.GetMaxPacketLength());
  EXPECT_EQ(kDefaultMaxPacketSize, packets_[0].packet->length());
}

TEST_P(QuicPacketGeneratorTest, ConsumeData_EmptyData) {
  EXPECT_DFATAL(generator_.ConsumeData(kHeadersStreamId, MakeIOVector(""), 0,
                                       false, MAY_FEC_PROTECT, nullptr),
                "Attempt to consume empty data without FIN.");
}

TEST_P(QuicPacketGeneratorTest,
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

TEST_P(QuicPacketGeneratorTest, ConsumeData_BatchOperations) {
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
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  generator_.FinishBatchOperations();
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_stream_frames = 2;
  CheckPacketContains(contents, 0);
}

TEST_P(QuicPacketGeneratorTest, ConsumeDataFecOnMaxGroupSize) {
  delegate_.SetCanWriteAnything();

  // Send FEC every two packets.
  creator_->set_max_packets_per_fec_group(2);

  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    if (generator_.fec_send_policy() == FEC_ALARM_TRIGGER) {
      // FEC packet is not sent when send policy is FEC_ALARM_TRIGGER, but FEC
      // group is closed.
      EXPECT_CALL(delegate_, OnResetFecGroup()).Times(1);
    } else {
      EXPECT_CALL(delegate_, OnSerializedPacket(_))
          .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    }
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  }

  // Send enough data to create 3 packets: two full and one partial. Send with
  // MUST_FEC_PROTECT flag.
  size_t data_len = 2 * kDefaultMaxPacketSize + 100;
  QuicConsumedData consumed = generator_.ConsumeData(
      3, CreateData(data_len), 0, true, MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  CheckPacketHasSingleStreamFrame(0);
  CheckPacketHasSingleStreamFrame(1);
  if (generator_.fec_send_policy() == FEC_ALARM_TRIGGER) {
    // FEC packet is not sent when send policy is FEC_ALARM_TRIGGER.
    CheckPacketHasSingleStreamFrame(2);
  } else {
    CheckPacketIsFec(2, 1);
    CheckPacketHasSingleStreamFrame(3);
  }
  EXPECT_TRUE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // If FEC send policy is FEC_ANY_TRIGGER, then the FEC packet under
  // construction will be sent when one more packet is sent (since FEC group
  // size is 2), or when OnFecTimeout is called. Send more data with
  // MAY_FEC_PROTECT. This packet should also be protected, and FEC packet is
  // sent since FEC group size is reached.
  //
  // If FEC send policy is FEC_ALARM_TRIGGER, FEC group is closed when the group
  // size is reached. FEC packet is not sent.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    if (generator_.fec_send_policy() == FEC_ALARM_TRIGGER) {
      EXPECT_CALL(delegate_, OnResetFecGroup()).Times(1);
    } else {
      EXPECT_CALL(delegate_, OnSerializedPacket(_))
          .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    }
  }
  consumed = generator_.ConsumeData(5, CreateData(1u), 0, true, MAY_FEC_PROTECT,
                                    nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  if (generator_.fec_send_policy() == FEC_ALARM_TRIGGER) {
    CheckPacketHasSingleStreamFrame(3);
  } else {
    CheckPacketHasSingleStreamFrame(4);
    CheckPacketIsFec(5, 4);
  }
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));
}

TEST_P(QuicPacketGeneratorTest, ConsumeDataSendsFecOnTimeout) {
  delegate_.SetCanWriteAnything();
  creator_->set_max_packets_per_fec_group(1000);

  // Send data with MUST_FEC_PROTECT flag. No FEC packet is emitted, but the
  // creator FEC protects all data.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  QuicConsumedData consumed = generator_.ConsumeData(3, CreateData(1u), 0, true,
                                                     MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  CheckPacketHasSingleStreamFrame(0);
  EXPECT_TRUE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // Send more data with MAY_FEC_PROTECT. This packet should also be protected,
  // and FEC packet is not yet sent.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  consumed = generator_.ConsumeData(5, CreateData(1u), 0, true, MAY_FEC_PROTECT,
                                    nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  CheckPacketHasSingleStreamFrame(1);
  EXPECT_TRUE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // Calling OnFecTimeout should cause the FEC packet to be emitted.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  generator_.OnFecTimeout();
  CheckPacketIsFec(2, 1);
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // Subsequent data is protected under the next FEC group. Send enough data to
  // create 2 more packets: one full and one partial.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  }
  size_t data_len = kDefaultMaxPacketSize + 1;
  consumed = generator_.ConsumeData(7, CreateData(data_len), 0, true,
                                    MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  CheckPacketHasSingleStreamFrame(3);
  CheckPacketHasSingleStreamFrame(4);
  EXPECT_TRUE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // Calling OnFecTimeout should cause the FEC packet to be emitted.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  generator_.OnFecTimeout();
  CheckPacketIsFec(5, 4);
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));
}

TEST_P(QuicPacketGeneratorTest, GetFecTimeoutFiniteOnlyOnFirstPacketInGroup) {
  delegate_.SetCanWriteAnything();
  creator_->set_max_packets_per_fec_group(6);

  // Send enough data to create 2 packets: one full and one partial. Send with
  // MUST_FEC_PROTECT flag. No FEC packet is emitted yet, but the creator FEC
  // protects all data.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  }
  size_t data_len = 1 * kDefaultMaxPacketSize + 100;
  QuicConsumedData consumed = generator_.ConsumeData(
      3, CreateData(data_len), 0, true, MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());
  CheckPacketHasSingleStreamFrame(0);
  CheckPacketHasSingleStreamFrame(1);
  EXPECT_TRUE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // GetFecTimeout returns finite timeout only for first packet in group.
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(kMinFecTimeoutMs),
            generator_.GetFecTimeout(/*packet_number=*/1u));
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            generator_.GetFecTimeout(/*packet_number=*/2u));

  // Send more data with MAY_FEC_PROTECT. This packet should also be protected,
  // and FEC packet is not yet sent.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  consumed = generator_.ConsumeData(5, CreateData(1u), 0, true, MAY_FEC_PROTECT,
                                    nullptr);
  CheckPacketHasSingleStreamFrame(2);
  EXPECT_TRUE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // GetFecTimeout returns finite timeout only for first packet in group.
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            generator_.GetFecTimeout(/*packet_number=*/3u));

  // Calling OnFecTimeout should cause the FEC packet to be emitted.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  generator_.OnFecTimeout();
  CheckPacketIsFec(3, /*fec_group=*/1u);
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // Subsequent data is protected under the next FEC group. Send enough data to
  // create 2 more packets: one full and one partial.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  }
  data_len = kDefaultMaxPacketSize + 1u;
  consumed = generator_.ConsumeData(7, CreateData(data_len), 0, true,
                                    MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  CheckPacketHasSingleStreamFrame(4);
  CheckPacketHasSingleStreamFrame(5);
  EXPECT_TRUE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // GetFecTimeout returns finite timeout for first packet in the new group.
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(kMinFecTimeoutMs),
            generator_.GetFecTimeout(/*packet_number=*/5u));
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            generator_.GetFecTimeout(/*packet_number=*/6u));

  // Calling OnFecTimeout should cause the FEC packet to be emitted.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  generator_.OnFecTimeout();
  CheckPacketIsFec(6, /*fec_group=*/5u);
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // Send more data with MAY_FEC_PROTECT. No FEC protection, so GetFecTimeout
  // returns infinite.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  consumed = generator_.ConsumeData(9, CreateData(1u), 0, true, MAY_FEC_PROTECT,
                                    nullptr);
  CheckPacketHasSingleStreamFrame(7);
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));
  EXPECT_EQ(QuicTime::Delta::Infinite(),
            generator_.GetFecTimeout(/*packet_number=*/8u));
}

TEST_P(QuicPacketGeneratorTest, ConsumeData_FramesPreviouslyQueued) {
  // Set the packet size be enough for two stream frames with 0 stream offset,
  // but not enough for a stream frame of 0 offset and one with non-zero offset.
  size_t length =
      NullEncrypter().GetCiphertextSize(0) +
      GetPacketHeaderSize(
          creator_->connection_id_length(), kIncludeVersion, !kIncludePathId,
          QuicPacketCreatorPeer::NextPacketNumberLength(creator_),
          NOT_IN_FEC_GROUP) +
      // Add an extra 3 bytes for the payload and 1 byte so BytesFree is larger
      // than the GetMinStreamFrameSize.
      QuicFramer::GetMinStreamFrameSize(1, 0, false, NOT_IN_FEC_GROUP) + 3 +
      QuicFramer::GetMinStreamFrameSize(1, 0, true, NOT_IN_FEC_GROUP) + 1;
  generator_.SetMaxPacketLength(length, /*force=*/false);
  delegate_.SetCanWriteAnything();
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
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
  // to be serialized, and it will be added to a new open packet.
  consumed = generator_.ConsumeData(kHeadersStreamId, MakeIOVector("bar"), 3,
                                    true, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedFrames());

  creator_->Flush();
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, 0);
  CheckPacketContains(contents, 1);
}

TEST_P(QuicPacketGeneratorTest, NoFecPacketSentWhenBatchEnds) {
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
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  generator_.FinishBatchOperations();
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_stream_frames = 2u;
  contents.fec_group = 1u;
  CheckPacketContains(contents, 0);

  // Forcing FEC timeout causes FEC packet to be emitted.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  generator_.OnFecTimeout();
  CheckPacketIsFec(1, /*fec_group=*/1u);
}

TEST_P(QuicPacketGeneratorTest, FecTimeoutOnRttChange) {
  EXPECT_EQ(QuicTime::Delta::Zero(),
            QuicPacketCreatorPeer::GetFecTimeout(creator_));
  generator_.OnRttChange(QuicTime::Delta::FromMilliseconds(300));
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(150),
            QuicPacketCreatorPeer::GetFecTimeout(creator_));
}

TEST_P(QuicPacketGeneratorTest, FecGroupSizeOnCongestionWindowChange) {
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

TEST_P(QuicPacketGeneratorTest, FecGroupSizeChangeWithOpenGroup) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();
  creator_->set_max_packets_per_fec_group(50);
  EXPECT_EQ(50u, creator_->max_packets_per_fec_group());
  EXPECT_FALSE(creator_->IsFecGroupOpen());

  // Send enough data to create 4 packets with MUST_FEC_PROTECT flag. 3 packets
  // are sent, one is queued in the creator.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  }
  size_t data_len = 3 * kDefaultMaxPacketSize + 1;
  QuicConsumedData consumed = generator_.ConsumeData(
      7, CreateData(data_len), 0, true, MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(creator_->IsFecGroupOpen());

  // Change FEC groupsize.
  generator_.OnCongestionWindowChange(2);
  EXPECT_EQ(2u, creator_->max_packets_per_fec_group());

  // If FEC send policy is FEC_ANY_TRIGGER, then send enough data to trigger one
  // unprotected data packet, causing the FEC packet to also be sent.
  //
  // If FEC send policy is FEC_ALARM_TRIGGER, FEC group is closed and FEC packet
  // is not sent.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    if (generator_.fec_send_policy() == FEC_ALARM_TRIGGER) {
      EXPECT_CALL(delegate_, OnResetFecGroup()).Times(1);
    } else {
      EXPECT_CALL(delegate_, OnSerializedPacket(_))
          .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    }
  }
  consumed = generator_.ConsumeData(7, CreateData(kDefaultMaxPacketSize), 0,
                                    true, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(kDefaultMaxPacketSize, consumed.bytes_consumed);
  if (generator_.fec_send_policy() == FEC_ANY_TRIGGER) {
    // Verify that one FEC packet was sent.
    CheckPacketIsFec(4, /*fec_group=*/1u);
  }
  EXPECT_FALSE(creator_->IsFecGroupOpen());
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));
}

TEST_P(QuicPacketGeneratorTest, SwitchFecOnOff) {
  delegate_.SetCanWriteAnything();
  creator_->set_max_packets_per_fec_group(2);
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // Send one unprotected data packet.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  QuicConsumedData consumed = generator_.ConsumeData(5, CreateData(1u), 0, true,
                                                     MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));
  // Verify that one data packet was sent.
  PacketContents contents;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, 0);

  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    if (generator_.fec_send_policy() == FEC_ALARM_TRIGGER) {
      // If FEC send policy is FEC_ALARM_TRIGGER, FEC group is closed.
      EXPECT_CALL(delegate_, OnResetFecGroup()).Times(1);
    } else {
      EXPECT_CALL(delegate_, OnSerializedPacket(_))
          .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    }
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  }
  // Send enough data to create 3 packets with MUST_FEC_PROTECT flag.
  size_t data_len = 2 * kDefaultMaxPacketSize + 100;
  consumed = generator_.ConsumeData(7, CreateData(data_len), 0, true,
                                    MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  // If FEC send policy is FEC_ANY_TRIGGER, verify that packets sent were 3 data
  // and 1 FEC.
  //
  // If FEC send policy is FEC_ALARM_TRIGGER, verify that packets sent were 3
  // data and FEC group is closed.
  CheckPacketHasSingleStreamFrame(1);
  CheckPacketHasSingleStreamFrame(2);
  if (generator_.fec_send_policy() == FEC_ALARM_TRIGGER) {
    CheckPacketHasSingleStreamFrame(3);
  } else {
    CheckPacketIsFec(3, /*fec_group=*/2u);
    CheckPacketHasSingleStreamFrame(4);
  }

  // Calling OnFecTimeout should emit the pending FEC packet.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  generator_.OnFecTimeout();
  if (generator_.fec_send_policy() == FEC_ALARM_TRIGGER) {
    CheckPacketIsFec(4, /*fec_group=*/4u);
  } else {
    CheckPacketIsFec(5, /*fec_group=*/5u);
  }

  // Send one unprotected data packet.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  consumed = generator_.ConsumeData(7, CreateData(1u), 0, true, MAY_FEC_PROTECT,
                                    nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));
  // Verify that one unprotected data packet was sent.
  if (generator_.fec_send_policy() == FEC_ALARM_TRIGGER) {
    CheckPacketContains(contents, 5);
  } else {
    CheckPacketContains(contents, 6);
  }
}

TEST_P(QuicPacketGeneratorTest, SwitchFecOnWithPendingFrameInCreator) {
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
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));
  consumed = generator_.ConsumeData(7, CreateData(1u), 0, true,
                                    MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  PacketContents contents;
  contents.num_stream_frames = 1;
  // Transmitted packet was not FEC protected.
  CheckPacketContains(contents, 0);
  EXPECT_TRUE(QuicPacketCreatorPeer::IsFecProtected(creator_));
  EXPECT_TRUE(creator_->HasPendingFrames());
}

TEST_P(QuicPacketGeneratorTest, SwitchFecOnWithPendingFramesInGenerator) {
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
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // Queue protected data for sending. Should cause queued frames to be flushed.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  QuicConsumedData consumed = generator_.ConsumeData(7, CreateData(1u), 0, true,
                                                     MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  PacketContents contents;
  contents.num_ack_frames = 1;
  contents.num_stop_waiting_frames = 1;
  CheckPacketContains(contents, 0);

  // FEC protection should be on in creator.
  EXPECT_TRUE(QuicPacketCreatorPeer::IsFecProtected(creator_));
}

TEST_P(QuicPacketGeneratorTest, SwitchFecOnOffWithSubsequentFramesProtected) {
  delegate_.SetCanWriteAnything();

  // Enable FEC.
  creator_->set_max_packets_per_fec_group(2);
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // Queue stream frame to be protected in creator.
  generator_.StartBatchOperations();
  QuicConsumedData consumed = generator_.ConsumeData(5, CreateData(1u), 0, true,
                                                     MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  // Creator has a pending protected frame.
  EXPECT_TRUE(creator_->HasPendingFrames());
  EXPECT_TRUE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // Add enough unprotected data to exceed size of current packet, so that
  // current packet is sent. Both frames will be sent out in a single packet.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  size_t data_len = kDefaultMaxPacketSize;
  consumed = generator_.ConsumeData(5, CreateData(data_len), 0, true,
                                    MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  PacketContents contents;
  contents.num_stream_frames = 2u;
  contents.fec_group = 1u;
  CheckPacketContains(contents, 0);
  // FEC protection should still be on in creator.
  EXPECT_TRUE(QuicPacketCreatorPeer::IsFecProtected(creator_));
}

TEST_P(QuicPacketGeneratorTest, SwitchFecOnOffWithSubsequentPacketsProtected) {
  delegate_.SetCanWriteAnything();

  // Enable FEC.
  creator_->set_max_packets_per_fec_group(2);
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // Send first packet, FEC protected.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  QuicConsumedData consumed = generator_.ConsumeData(5, CreateData(1u), 0, true,
                                                     MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  PacketContents contents;
  contents.num_stream_frames = 1u;
  contents.fec_group = 1u;
  CheckPacketContains(contents, 0);

  // FEC should still be on in creator.
  EXPECT_TRUE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // Send unprotected data to cause second packet to be sent, which gets
  // protected because it happens to fall within an open FEC group. Data packet
  // will be followed by FEC packet.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    if (generator_.fec_send_policy() == FEC_ALARM_TRIGGER) {
      EXPECT_CALL(delegate_, OnResetFecGroup()).Times(1);
    } else {
      EXPECT_CALL(delegate_, OnSerializedPacket(_))
          .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    }
  }
  consumed = generator_.ConsumeData(5, CreateData(1u), 0, true, MAY_FEC_PROTECT,
                                    nullptr);
  EXPECT_EQ(1u, consumed.bytes_consumed);
  contents.num_stream_frames = 1u;
  CheckPacketContains(contents, 1);
  if (generator_.fec_send_policy() == FEC_ANY_TRIGGER) {
    // FEC packet is sent when send policy is FEC_ANY_TRIGGER.
    CheckPacketIsFec(2, /*fec_group=*/1u);
  }

  // FEC protection should be off in creator.
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));
}

TEST_P(QuicPacketGeneratorTest, SwitchFecOnOffThenOnWithCreatorProtectionOn) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  // Enable FEC.
  creator_->set_max_packets_per_fec_group(2);
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // Queue one byte of FEC protected data.
  QuicConsumedData consumed = generator_.ConsumeData(5, CreateData(1u), 0, true,
                                                     MUST_FEC_PROTECT, nullptr);
  EXPECT_TRUE(creator_->HasPendingFrames());

  // Add more unprotected data causing first packet to be sent, FEC protected.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  size_t data_len = kDefaultMaxPacketSize;
  consumed = generator_.ConsumeData(5, CreateData(data_len), 0, true,
                                    MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  PacketContents contents;
  contents.num_stream_frames = 2u;
  contents.fec_group = 1u;
  CheckPacketContains(contents, 0);

  // FEC group is still open in creator.
  EXPECT_TRUE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // Add data that should be protected, large enough to cause second packet to
  // be sent. Data packet should be followed by FEC packet.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    if (generator_.fec_send_policy() == FEC_ALARM_TRIGGER) {
      EXPECT_CALL(delegate_, OnResetFecGroup()).Times(1);
    } else {
      EXPECT_CALL(delegate_, OnSerializedPacket(_))
          .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    }
  }
  consumed = generator_.ConsumeData(5, CreateData(data_len), 0, true,
                                    MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  CheckPacketContains(contents, 1);
  if (generator_.fec_send_policy() == FEC_ANY_TRIGGER) {
    // FEC packet is sent when send policy is FEC_ANY_TRIGGER.
    CheckPacketIsFec(2, /*fec_group=*/1u);
  }

  // FEC protection should remain on in creator.
  EXPECT_TRUE(QuicPacketCreatorPeer::IsFecProtected(creator_));
}

TEST_P(QuicPacketGeneratorTest, ResetFecGroupNoTimeout) {
  delegate_.SetCanWriteAnything();
  // Send FEC packet after 2 packets.
  creator_->set_max_packets_per_fec_group(2);
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // Send two packets so that when this data is consumed, two packets are sent
  // out. In FEC_TRIGGER_ANY, this will cause an FEC packet to be sent out and
  // with FEC_TRIGGER_ALARM, this will cause a Reset to be called. In both
  // cases, the creator's fec protection will be turned off afterwards.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    if (generator_.fec_send_policy() == FEC_ALARM_TRIGGER) {
      // FEC packet is not sent when send policy is FEC_ALARM_TRIGGER, but FEC
      // group is closed.
      EXPECT_CALL(delegate_, OnResetFecGroup()).Times(1);
    } else {
      EXPECT_CALL(delegate_, OnSerializedPacket(_))
          .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    }
    // Fin Packet.
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  }
  size_t data_len = 2 * kDefaultMaxPacketSize;
  QuicConsumedData consumed = generator_.ConsumeData(
      5, CreateData(data_len), 0, true, MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());
  CheckPacketHasSingleStreamFrame(0);
  CheckPacketHasSingleStreamFrame(1);
  if (generator_.fec_send_policy() == FEC_ALARM_TRIGGER) {
    // FEC packet is not sent when send policy is FEC_ALARM_TRIGGER.
    CheckPacketHasSingleStreamFrame(2);
  } else {
    // FEC packet is sent after 2 packets and when send policy is
    // FEC_ANY_TRIGGER.
    CheckPacketIsFec(2, 1);
    CheckPacketHasSingleStreamFrame(3);
  }
  EXPECT_TRUE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // Do the same send  (with MUST_FEC_PROTECT) on a different stream id.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    // FEC packet is sent after 2 packets and when send policy is
    // FEC_ANY_TRIGGER. When policy is FEC_ALARM_TRIGGER, FEC group is closed
    // and FEC packet is not sent.
    if (generator_.fec_send_policy() == FEC_ALARM_TRIGGER) {
      EXPECT_CALL(delegate_, OnResetFecGroup()).Times(1);
    } else {
      EXPECT_CALL(delegate_, OnSerializedPacket(_))
          .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    }
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    // FEC packet is sent after 2 packets and when send policy is
    // FEC_ANY_TRIGGER. When policy is FEC_ALARM_TRIGGER, FEC group is closed
    // and FEC packet is not sent.
    if (generator_.fec_send_policy() == FEC_ALARM_TRIGGER) {
      EXPECT_CALL(delegate_, OnResetFecGroup()).Times(1);
    } else {
      EXPECT_CALL(delegate_, OnSerializedPacket(_))
          .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    }
  }
  consumed = generator_.ConsumeData(7, CreateData(data_len), 0, true,
                                    MUST_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());
  if (generator_.fec_send_policy() == FEC_ALARM_TRIGGER) {
    CheckPacketHasSingleStreamFrame(3);
    CheckPacketHasSingleStreamFrame(4);
    CheckPacketHasSingleStreamFrame(5);
  } else {
    CheckPacketHasSingleStreamFrame(4);
    // FEC packet is sent after 2 packets and when send policy is
    // FEC_ANY_TRIGGER.
    CheckPacketIsFec(5, 4);
    CheckPacketHasSingleStreamFrame(6);
    CheckPacketHasSingleStreamFrame(7);
    // FEC packet is sent after 2 packets and when send policy is
    // FEC_ANY_TRIGGER.
    CheckPacketIsFec(8, 7);
  }
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  // Do the another send (with MAY_FEC_PROTECT) on a different stream id, which
  // should not produce an FEC packet because the last FEC group has been
  // closed.
  {
    InSequence dummy;
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  }
  consumed = generator_.ConsumeData(9, CreateData(data_len), 0, true,
                                    MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());
  if (generator_.fec_send_policy() == FEC_ALARM_TRIGGER) {
    CheckPacketHasSingleStreamFrame(6);
    CheckPacketHasSingleStreamFrame(7);
    CheckPacketHasSingleStreamFrame(8);
  } else {
    CheckPacketHasSingleStreamFrame(9);
    CheckPacketHasSingleStreamFrame(10);
    CheckPacketHasSingleStreamFrame(11);
  }
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));
}

// 1. Create and send one packet with MUST_FEC_PROTECT.
// 2. Call FecTimeout, expect FEC packet is sent.
// 3. Do the same thing over again, with a different stream id.
TEST_P(QuicPacketGeneratorTest, FecPacketSentOnFecTimeout) {
  delegate_.SetCanWriteAnything();
  creator_->set_max_packets_per_fec_group(1000);
  EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));

  for (int i = 1; i < 4; i = i + 2) {
    // Send data with MUST_FEC_PROTECT flag. No FEC packet is emitted, but the
    // creator FEC protects all data.
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    QuicConsumedData consumed = generator_.ConsumeData(
        i + 2, CreateData(1u), 0, true, MUST_FEC_PROTECT, nullptr);
    EXPECT_EQ(1u, consumed.bytes_consumed);
    EXPECT_TRUE(consumed.fin_consumed);
    CheckPacketHasSingleStreamFrame(0);
    EXPECT_TRUE(QuicPacketCreatorPeer::IsFecProtected(creator_));

    // Calling OnFecTimeout should cause the FEC packet to be emitted.
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    generator_.OnFecTimeout();
    CheckPacketIsFec(i, i);
    EXPECT_FALSE(QuicPacketCreatorPeer::IsFecProtected(creator_));
  }
}

TEST_P(QuicPacketGeneratorTest, NotWritableThenBatchOperations) {
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
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  generator_.FinishBatchOperations();
  EXPECT_FALSE(generator_.HasQueuedFrames());

  PacketContents contents;
  contents.num_ack_frames = 1;
  contents.num_goaway_frames = 1;
  contents.num_rst_stream_frames = 1;
  contents.num_stream_frames = 1;
  CheckPacketContains(contents, 0);
}

TEST_P(QuicPacketGeneratorTest, NotWritableThenBatchOperations2) {
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
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
    EXPECT_CALL(delegate_, OnSerializedPacket(_))
        .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
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
  CheckPacketContains(contents, 0);

  // The second should have the remainder of the stream data.
  PacketContents contents2;
  contents2.num_goaway_frames = 1;
  contents2.num_stream_frames = 1;
  CheckPacketContains(contents2, 1);
}

TEST_P(QuicPacketGeneratorTest, TestConnectionIdLength) {
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

// Test whether SetMaxPacketLength() works in the situation when the queue is
// empty, and we send three packets worth of data.
TEST_P(QuicPacketGeneratorTest, SetMaxPacketLength_Initial) {
  delegate_.SetCanWriteAnything();

  // Send enough data for three packets.
  size_t data_len = 3 * kDefaultMaxPacketSize + 1;
  size_t packet_len = kDefaultMaxPacketSize + 100;
  ASSERT_LE(packet_len, kMaxPacketSize);
  generator_.SetMaxPacketLength(packet_len, /*force=*/false);
  EXPECT_EQ(packet_len, generator_.GetCurrentMaxPacketLength());

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .Times(3)
      .WillRepeatedly(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  QuicConsumedData consumed =
      generator_.ConsumeData(kHeadersStreamId, CreateData(data_len),
                             /*offset=*/2,
                             /*fin=*/true, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  // We expect three packets, and first two of them have to be of packet_len
  // size.  We check multiple packets (instead of just one) because we want to
  // ensure that |max_packet_length_| does not get changed incorrectly by the
  // generator after first packet is serialized.
  ASSERT_EQ(3u, packets_.size());
  EXPECT_EQ(packet_len, packets_[0].packet->length());
  EXPECT_EQ(packet_len, packets_[1].packet->length());
  CheckAllPacketsHaveSingleStreamFrame();
}

// Test whether SetMaxPacketLength() works in the situation when we first write
// data, then change packet size, then write data again.
TEST_P(QuicPacketGeneratorTest, SetMaxPacketLength_Middle) {
  delegate_.SetCanWriteAnything();

  // We send enough data to overflow default packet length, but not the altered
  // one.
  size_t data_len = kDefaultMaxPacketSize;
  size_t packet_len = kDefaultMaxPacketSize + 100;
  ASSERT_LE(packet_len, kMaxPacketSize);

  // We expect to see three packets in total.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .Times(3)
      .WillRepeatedly(Invoke(this, &QuicPacketGeneratorTest::SavePacket));

  // Send two packets before packet size change.
  QuicConsumedData consumed =
      generator_.ConsumeData(kHeadersStreamId, CreateData(data_len),
                             /*offset=*/2,
                             /*fin=*/false, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  // Make sure we already have two packets.
  ASSERT_EQ(2u, packets_.size());

  // Increase packet size.
  generator_.SetMaxPacketLength(packet_len, /*force=*/false);
  EXPECT_EQ(packet_len, generator_.GetCurrentMaxPacketLength());

  // Send a packet after packet size change.
  consumed = generator_.ConsumeData(kHeadersStreamId, CreateData(data_len),
                                    2 + data_len,
                                    /*fin=*/true, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  // We expect first data chunk to get fragmented, but the second one to fit
  // into a single packet.
  ASSERT_EQ(3u, packets_.size());
  EXPECT_EQ(kDefaultMaxPacketSize, packets_[0].packet->length());
  EXPECT_LE(kDefaultMaxPacketSize, packets_[2].packet->length());
  CheckAllPacketsHaveSingleStreamFrame();
}

// Test whether SetMaxPacketLength() works correctly when we change the packet
// size in the middle of the batched packet.
TEST_P(QuicPacketGeneratorTest, SetMaxPacketLength_Midpacket) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  size_t first_write_len = kDefaultMaxPacketSize / 2;
  size_t second_write_len = kDefaultMaxPacketSize;
  size_t packet_len = kDefaultMaxPacketSize + 100;
  ASSERT_LE(packet_len, kMaxPacketSize);

  // First send half of the packet worth of data.  We are in the batch mode, so
  // should not cause packet serialization.
  QuicConsumedData consumed =
      generator_.ConsumeData(kHeadersStreamId, CreateData(first_write_len),
                             /*offset=*/2,
                             /*fin=*/false, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(first_write_len, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedFrames());

  // Make sure we have no packets so far.
  ASSERT_EQ(0u, packets_.size());

  // Increase packet size.  Ensure it's not immediately enacted.
  generator_.SetMaxPacketLength(packet_len, /*force=*/false);
  EXPECT_EQ(packet_len, generator_.GetMaxPacketLength());
  EXPECT_EQ(kDefaultMaxPacketSize, generator_.GetCurrentMaxPacketLength());

  // We expect to see exactly one packet serialized after that, since we are in
  // batch mode and we have sent approximately 3/2 of our MTU.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));

  // Send a packet worth of data to the same stream.  This should trigger
  // serialization of other packet.
  consumed =
      generator_.ConsumeData(kHeadersStreamId, CreateData(second_write_len),
                             /*offset=*/2 + first_write_len,
                             /*fin=*/true, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(second_write_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedFrames());

  // We expect the first packet to contain two frames, and to not reflect the
  // packet size change.
  ASSERT_EQ(1u, packets_.size());
  EXPECT_EQ(kDefaultMaxPacketSize, packets_[0].packet->length());

  PacketContents contents;
  contents.num_stream_frames = 2;
  CheckPacketContains(contents, 0);
}

// Test whether SetMaxPacketLength() works correctly when we force the change of
// the packet size in the middle of the batched packet.
TEST_P(QuicPacketGeneratorTest, SetMaxPacketLength_MidpacketFlush) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  size_t first_write_len = kDefaultMaxPacketSize / 2;
  size_t packet_len = kDefaultMaxPacketSize + 100;
  size_t second_write_len = packet_len + 1;
  ASSERT_LE(packet_len, kMaxPacketSize);

  // First send half of the packet worth of data.  We are in the batch mode, so
  // should not cause packet serialization.
  QuicConsumedData consumed =
      generator_.ConsumeData(kHeadersStreamId, CreateData(first_write_len),
                             /*offset=*/2,
                             /*fin=*/false, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(first_write_len, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedFrames());

  // Make sure we have no packets so far.
  ASSERT_EQ(0u, packets_.size());

  // Expect a packet to be flushed.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));

  // Increase packet size.  Ensure it's immediately enacted.
  generator_.SetMaxPacketLength(packet_len, /*force=*/true);
  EXPECT_EQ(packet_len, generator_.GetMaxPacketLength());
  EXPECT_EQ(packet_len, generator_.GetCurrentMaxPacketLength());
  EXPECT_FALSE(generator_.HasQueuedFrames());

  // We expect to see exactly one packet serialized after that, because we send
  // a value somewhat exceeding new max packet size, and the tail data does not
  // get serialized because we are still in the batch mode.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));

  // Send a more than a packet worth of data to the same stream.  This should
  // trigger serialization of one packet, and queue another one.
  consumed =
      generator_.ConsumeData(kHeadersStreamId, CreateData(second_write_len),
                             /*offset=*/2 + first_write_len,
                             /*fin=*/true, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(second_write_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedFrames());

  // We expect the first packet to be underfilled, and the second packet be up
  // to the new max packet size.
  ASSERT_EQ(2u, packets_.size());
  EXPECT_GT(kDefaultMaxPacketSize, packets_[0].packet->length());
  EXPECT_EQ(packet_len, packets_[1].packet->length());

  CheckAllPacketsHaveSingleStreamFrame();
}

// Test sending an MTU probe, without any surrounding data.
TEST_P(QuicPacketGeneratorTest, GenerateMtuDiscoveryPacket_Simple) {
  delegate_.SetCanWriteAnything();

  const size_t target_mtu = kDefaultMaxPacketSize + 100;
  static_assert(target_mtu < kMaxPacketSize,
                "The MTU probe used by the test exceeds maximum packet size");

  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));

  generator_.GenerateMtuDiscoveryPacket(target_mtu, nullptr);

  EXPECT_FALSE(generator_.HasQueuedFrames());
  ASSERT_EQ(1u, packets_.size());
  EXPECT_EQ(target_mtu, packets_[0].packet->length());

  PacketContents contents;
  contents.num_mtu_discovery_frames = 1;
  CheckPacketContains(contents, 0);
}

// Test sending an MTU probe.  Surround it with data, to ensure that it resets
// the MTU to the value before the probe was sent.
TEST_P(QuicPacketGeneratorTest, GenerateMtuDiscoveryPacket_SurroundedByData) {
  delegate_.SetCanWriteAnything();

  const size_t target_mtu = kDefaultMaxPacketSize + 100;
  static_assert(target_mtu < kMaxPacketSize,
                "The MTU probe used by the test exceeds maximum packet size");

  // Send enough data so it would always cause two packets to be sent.
  const size_t data_len = target_mtu + 1;

  // Send a total of five packets: two packets before the probe, the probe
  // itself, and two packets after the probe.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .Times(5)
      .WillRepeatedly(Invoke(this, &QuicPacketGeneratorTest::SavePacket));

  // Send data before the MTU probe.
  QuicConsumedData consumed =
      generator_.ConsumeData(kHeadersStreamId, CreateData(data_len),
                             /*offset=*/2,
                             /*fin=*/false, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  // Send the MTU probe.
  generator_.GenerateMtuDiscoveryPacket(target_mtu, nullptr);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  // Send data after the MTU probe.
  consumed = generator_.ConsumeData(kHeadersStreamId, CreateData(data_len),
                                    /*offset=*/2 + data_len,
                                    /*fin=*/true, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(data_len, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_FALSE(generator_.HasQueuedFrames());

  ASSERT_EQ(5u, packets_.size());
  EXPECT_EQ(kDefaultMaxPacketSize, packets_[0].packet->length());
  EXPECT_EQ(target_mtu, packets_[2].packet->length());
  EXPECT_EQ(kDefaultMaxPacketSize, packets_[3].packet->length());

  PacketContents probe_contents;
  probe_contents.num_mtu_discovery_frames = 1;

  CheckPacketHasSingleStreamFrame(0);
  CheckPacketHasSingleStreamFrame(1);
  CheckPacketContains(probe_contents, 2);
  CheckPacketHasSingleStreamFrame(3);
  CheckPacketHasSingleStreamFrame(4);
}

TEST_P(QuicPacketGeneratorTest, DontCrashOnInvalidStopWaiting) {
  // Test added to ensure the generator does not crash when an invalid frame is
  // added.  Because this is an indication of internal programming errors,
  // DFATALs are expected.
  // A 1 byte packet number length can't encode a gap of 1000.
  QuicPacketCreatorPeer::SetPacketNumber(creator_, 1000);

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

  // This will not serialize any packets, because of the invalid frame.
  EXPECT_CALL(delegate_,
              CloseConnection(QUIC_FAILED_TO_SERIALIZE_PACKET, false));
  EXPECT_DFATAL(generator_.FinishBatchOperations(),
                "packet_number_length 1 is too small "
                "for least_unacked_delta: 1001");
}

TEST_P(QuicPacketGeneratorTest, SetCurrentPath) {
  delegate_.SetCanWriteAnything();
  generator_.StartBatchOperations();

  QuicConsumedData consumed = generator_.ConsumeData(
      kHeadersStreamId, MakeIOVector("foo"), 2, true, MAY_FEC_PROTECT, nullptr);
  EXPECT_EQ(3u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_TRUE(generator_.HasQueuedFrames());
  EXPECT_EQ(kDefaultPathId, QuicPacketCreatorPeer::GetCurrentPath(creator_));
  // Does not change current path.
  generator_.SetCurrentPath(kDefaultPathId, 1, 0);
  EXPECT_EQ(kDefaultPathId, QuicPacketCreatorPeer::GetCurrentPath(creator_));

  // Try to switch path when a packet is under construction.
  QuicPathId kTestPathId1 = 1;
  EXPECT_DFATAL(generator_.SetCurrentPath(kTestPathId1, 1, 0),
                "Unable to change paths when a packet is under construction");
  EXPECT_EQ(kDefaultPathId, QuicPacketCreatorPeer::GetCurrentPath(creator_));

  // Try to switch path after current open packet gets serialized.
  EXPECT_CALL(delegate_, OnSerializedPacket(_))
      .WillOnce(Invoke(this, &QuicPacketGeneratorTest::SavePacket));
  generator_.FlushAllQueuedFrames();
  EXPECT_FALSE(generator_.HasQueuedFrames());
  generator_.SetCurrentPath(kTestPathId1, 1, 0);
  EXPECT_EQ(kTestPathId1, QuicPacketCreatorPeer::GetCurrentPath(creator_));
}

}  // namespace test
}  // namespace net
