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
#include "net/quic/test_tools/quic_packet_creator_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::StringPiece;
using std::string;
using std::vector;
using testing::DoAll;
using testing::InSequence;
using testing::Return;
using testing::SaveArg;
using testing::_;

namespace net {
namespace test {
namespace {

class QuicPacketCreatorTest : public ::testing::TestWithParam<bool> {
 protected:
  QuicPacketCreatorTest()
      : server_framer_(kQuicVersion1,
                       QuicDecrypter::Create(kNULL),
                       QuicEncrypter::Create(kNULL),
                       QuicTime::Zero(),
                       true),
        client_framer_(kQuicVersion1,
                       QuicDecrypter::Create(kNULL),
                       QuicEncrypter::Create(kNULL),
                       QuicTime::Zero(),
                       false),
        id_(1),
        sequence_number_(0),
        guid_(2),
        data_("foo"),
        creator_(guid_, &client_framer_, QuicRandom::GetInstance(), false) {
    client_framer_.set_visitor(&framer_visitor_);
    server_framer_.set_visitor(&framer_visitor_);
  }
  ~QuicPacketCreatorTest() {
  }

  void ProcessPacket(QuicPacket* packet) {
    scoped_ptr<QuicEncryptedPacket> encrypted(
        server_framer_.EncryptPacket(sequence_number_, *packet));
    server_framer_.ProcessPacket(*encrypted);
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
  QuicFramer server_framer_;
  QuicFramer client_framer_;
  testing::StrictMock<MockFramerVisitor> framer_visitor_;
  QuicStreamId id_;
  QuicPacketSequenceNumber sequence_number_;
  QuicGuid guid_;
  string data_;
  QuicPacketCreator creator_;
};

TEST_F(QuicPacketCreatorTest, SerializeFrames) {
  frames_.push_back(QuicFrame(new QuicAckFrame(0u, QuicTime::Zero(), 0u)));
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
  frame.ack_frame = QuicAckFrame(0u, QuicTime::Zero(), 0u);

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

TEST_F(QuicPacketCreatorTest, SerializeVersionNegotiationPacket) {
  QuicPacketCreatorPeer::SetIsServer(&creator_, true);
  QuicVersionTagList versions;
  versions.push_back(kQuicVersion1);
  scoped_ptr<QuicEncryptedPacket> encrypted(
      creator_.SerializeVersionNegotiationPacket(versions));

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnVersionNegotiationPacket(_));
  }
  client_framer_.ProcessPacket(*encrypted.get());
}

INSTANTIATE_TEST_CASE_P(ToggleVersionSerialization,
                        QuicPacketCreatorTest,
                        ::testing::Values(false, true));

TEST_P(QuicPacketCreatorTest, SerializeFrame) {
  if (!GetParam()) {
    creator_.StopSendingVersion();
  }
  frames_.push_back(QuicFrame(new QuicStreamFrame(
      0u, false, 0u, StringPiece(""))));
  SerializedPacket serialized = creator_.SerializeAllFrames(frames_);
  delete frames_[0].stream_frame;

  QuicPacketHeader header;
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_)).WillOnce(
        DoAll(SaveArg<0>(&header), Return(true)));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  EXPECT_EQ(GetParam(), header.public_header.version_flag);
  delete serialized.packet;
}

TEST_P(QuicPacketCreatorTest, CreateStreamFrameTooLarge) {
  if (!GetParam()) {
    creator_.StopSendingVersion();
  }
  // A string larger than fits into a frame.
  creator_.options()->max_packet_length = GetPacketLengthForOneStream(
      QuicPacketCreatorPeer::SendVersionInPacket(&creator_), 1);
  QuicFrame frame;
  size_t consumed = creator_.CreateStreamFrame(1u, "test", 0u, true, &frame);
  EXPECT_EQ(1u, consumed);
  CheckStreamFrame(frame, 1u, "t", 0u, false);
  delete frame.stream_frame;
}

TEST_P(QuicPacketCreatorTest, AddFrameAndSerialize) {
  if (!GetParam()) {
    creator_.StopSendingVersion();
  }
  const size_t max_plaintext_size =
      client_framer_.GetMaxPlaintextSize(creator_.options()->max_packet_length);
  EXPECT_FALSE(creator_.HasPendingFrames());
  EXPECT_EQ(max_plaintext_size -
            GetPacketHeaderSize(
                QuicPacketCreatorPeer::SendVersionInPacket(&creator_)),
            creator_.BytesFree());

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
  EXPECT_EQ(max_plaintext_size -
            GetPacketHeaderSize(
                QuicPacketCreatorPeer::SendVersionInPacket(&creator_)),
            creator_.BytesFree());
}

}  // namespace
}  // namespace test
}  // namespace net
