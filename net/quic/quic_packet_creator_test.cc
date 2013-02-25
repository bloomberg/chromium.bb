// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_creator.h"

#include "base/stl_util.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::StringPiece;
using std::string;
using std::vector;
using testing::InSequence;
using testing::_;

namespace net {
namespace test {
namespace {

class QuicPacketCreatorTest : public ::testing::Test {
 protected:
  QuicPacketCreatorTest()
      : framer_(QuicDecrypter::Create(kNULL), QuicEncrypter::Create(kNULL)),
        id_(1),
        sequence_number_(0),
        guid_(2),
        data_("foo"),
        creator_(guid_, &framer_, QuicRandom::GetInstance()) {
    framer_.set_visitor(&framer_visitor_);
  }
  ~QuicPacketCreatorTest() {
  }

  void ProcessPacket(QuicPacket* packet) {
    scoped_ptr<QuicEncryptedPacket> encrypted(
        framer_.EncryptPacket(sequence_number_, *packet));
    framer_.ProcessPacket(*encrypted);
  }

  void CheckStreamFrame(const QuicFrame& frame, QuicStreamId stream_id,
                        const string& data, QuicStreamOffset offset, bool fin) {
    EXPECT_EQ(STREAM_FRAME, frame.type);
    ASSERT_TRUE(frame.stream_frame);
    EXPECT_EQ(stream_id, frame.stream_frame->stream_id);
    EXPECT_EQ(data, frame.stream_frame->data);
    EXPECT_EQ(offset, frame.stream_frame->offset);
    EXPECT_EQ(fin, frame.stream_frame->fin);
  }

  QuicFrames frames_;
  QuicFramer framer_;
  testing::StrictMock<MockFramerVisitor> framer_visitor_;
  QuicStreamId id_;
  QuicPacketSequenceNumber sequence_number_;
  QuicGuid guid_;
  string data_;
  QuicPacketCreator creator_;
};

TEST_F(QuicPacketCreatorTest, SerializeFrame) {
  frames_.push_back(QuicFrame(new QuicStreamFrame(
      0u, false, 0u, StringPiece(""))));
  SerializedPacket serialized = creator_.SerializeAllFrames(frames_);
  delete frames_[0].stream_frame;

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;
}

TEST_F(QuicPacketCreatorTest, SerializeFrames) {
  frames_.push_back(QuicFrame(new QuicAckFrame(0u, 0u)));
  frames_.push_back(QuicFrame(new QuicStreamFrame(
      0u, false, 0u, StringPiece(""))));
  frames_.push_back(QuicFrame(new QuicStreamFrame(
      0u, true, 0u, StringPiece(""))));
  SerializedPacket serialized = creator_.SerializeAllFrames(frames_);
  delete frames_[0].ack_frame;
  delete frames_[1].stream_frame;
  delete frames_[2].stream_frame;

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnAckFrame(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;
}

TEST_F(QuicPacketCreatorTest, SerializeWithFEC) {
  creator_.options()->max_packets_per_fec_group = 6;
  ASSERT_FALSE(creator_.ShouldSendFec(false));
  creator_.MaybeStartFEC();

  frames_.push_back(QuicFrame(new QuicStreamFrame(
      0u, false, 0u, StringPiece(""))));
  SerializedPacket serialized = creator_.SerializeAllFrames(frames_);
  delete frames_[0].stream_frame;

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnFecProtectedPayload(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;

  ASSERT_FALSE(creator_.ShouldSendFec(false));
  ASSERT_TRUE(creator_.ShouldSendFec(true));

  serialized = creator_.SerializeFec();
  ASSERT_EQ(2u, serialized.sequence_number);

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnFecData(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;
}

TEST_F(QuicPacketCreatorTest, SerializeConnectionClose) {
  QuicConnectionCloseFrame frame;
  frame.error_code = QUIC_NO_ERROR;
  frame.ack_frame = QuicAckFrame(0u, 0u);

  SerializedPacket serialized = creator_.SerializeConnectionClose(&frame);
  ASSERT_EQ(1u, serialized.sequence_number);
  ASSERT_EQ(1u, creator_.sequence_number());

  InSequence s;
  EXPECT_CALL(framer_visitor_, OnPacket());
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnAckFrame(_));
  EXPECT_CALL(framer_visitor_, OnConnectionCloseFrame(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  ProcessPacket(serialized.packet);
  delete serialized.packet;
}

TEST_F(QuicPacketCreatorTest, CreateStreamFrame) {
  QuicFrame frame;
  size_t consumed = creator_.CreateStreamFrame(1u, "test", 0u, false, &frame);
  EXPECT_EQ(4u, consumed);
  CheckStreamFrame(frame, 1u, "test", 0u, false);
  delete frame.stream_frame;
}

TEST_F(QuicPacketCreatorTest, CreateStreamFrameFin) {
  QuicFrame frame;
  size_t consumed = creator_.CreateStreamFrame(1u, "test", 10u, true, &frame);
  EXPECT_EQ(4u, consumed);
  CheckStreamFrame(frame, 1u, "test", 10u, true);
  delete frame.stream_frame;
}

TEST_F(QuicPacketCreatorTest, CreateStreamFrameFinOnly) {
  QuicFrame frame;
  size_t consumed = creator_.CreateStreamFrame(1u, "", 0u, true, &frame);
  EXPECT_EQ(0u, consumed);
  CheckStreamFrame(frame, 1u, "", 0u, true);
  delete frame.stream_frame;
}

TEST_F(QuicPacketCreatorTest, CreateStreamFrameTooLarge) {
  // A string larger than fits into a frame.
  size_t ciphertext_size = NullEncrypter().GetCiphertextSize(1);
  creator_.options()->max_packet_length =
      ciphertext_size + QuicUtils::StreamFramePacketOverhead(1);
  QuicFrame frame;
  size_t consumed = creator_.CreateStreamFrame(1u, "test", 0u, true, &frame);
  EXPECT_EQ(1u, consumed);
  CheckStreamFrame(frame, 1u, "t", 0u, false);
  delete frame.stream_frame;
}

TEST_F(QuicPacketCreatorTest, AddFrameAndSerialize) {
  const size_t max_plaintext_size =
      framer_.GetMaxPlaintextSize(creator_.options()->max_packet_length);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_EQ(max_plaintext_size - kPacketHeaderSize, creator_.BytesFree());

  // Add a variety of frame types and then a padding frame.
  QuicAckFrame ack_frame;
  EXPECT_TRUE(creator_.AddSavedFrame(QuicFrame(&ack_frame)));
  EXPECT_TRUE(creator_.HasPendingFrames());

  QuicCongestionFeedbackFrame congestion_feedback;
  congestion_feedback.type = kFixRate;
  EXPECT_TRUE(creator_.AddSavedFrame(QuicFrame(&congestion_feedback)));
  EXPECT_TRUE(creator_.HasPendingFrames());

  QuicFrame frame;
  size_t consumed = creator_.CreateStreamFrame(1u, "test", 0u, false, &frame);
  EXPECT_EQ(4u, consumed);
  ASSERT_TRUE(frame.stream_frame);
  EXPECT_TRUE(creator_.AddSavedFrame(frame));
  EXPECT_TRUE(creator_.HasPendingFrames());

  QuicPaddingFrame padding_frame;
  EXPECT_TRUE(creator_.AddSavedFrame(QuicFrame(&padding_frame)));
  EXPECT_TRUE(creator_.HasPendingFrames());
  EXPECT_EQ(0u, creator_.BytesFree());

  EXPECT_FALSE(creator_.AddSavedFrame(QuicFrame(&ack_frame)));

  // Ensure the packet is successfully created.
  SerializedPacket serialized = creator_.SerializePacket();
  ASSERT_TRUE(serialized.packet);
  delete serialized.packet;
  ASSERT_TRUE(serialized.retransmittable_frames);
  RetransmittableFrames* retransmittable = serialized.retransmittable_frames;
  ASSERT_EQ(1u, retransmittable->frames().size());
  EXPECT_EQ(STREAM_FRAME, retransmittable->frames()[0].type);
  ASSERT_TRUE(retransmittable->frames()[0].stream_frame);
  delete serialized.retransmittable_frames;

  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_EQ(max_plaintext_size - kPacketHeaderSize, creator_.BytesFree());
}

}  // namespace
}  // namespace test
}  // namespace net
