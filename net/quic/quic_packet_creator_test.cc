// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_creator.h"

#include "base/stl_util.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/mock_random.h"
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
      : server_framer_(QuicSupportedVersions(), QuicTime::Zero(), true),
        client_framer_(QuicSupportedVersions(), QuicTime::Zero(), false),
        sequence_number_(0),
        connection_id_(2),
        data_("foo"),
        creator_(connection_id_, &client_framer_, &mock_random_, false) {
    client_framer_.set_visitor(&framer_visitor_);
    server_framer_.set_visitor(&framer_visitor_);
  }
  ~QuicPacketCreatorTest() {
  }

  void ProcessPacket(QuicPacket* packet) {
    scoped_ptr<QuicEncryptedPacket> encrypted(
        server_framer_.EncryptPacket(ENCRYPTION_NONE, sequence_number_,
                                     *packet));
    server_framer_.ProcessPacket(*encrypted);
  }

  void CheckStreamFrame(const QuicFrame& frame,
                        QuicStreamId stream_id,
                        const string& data,
                        QuicStreamOffset offset,
                        bool fin) {
    EXPECT_EQ(STREAM_FRAME, frame.type);
    ASSERT_TRUE(frame.stream_frame);
    EXPECT_EQ(stream_id, frame.stream_frame->stream_id);
    scoped_ptr<string> frame_data(frame.stream_frame->GetDataAsString());
    EXPECT_EQ(data, *frame_data);
    EXPECT_EQ(offset, frame.stream_frame->offset);
    EXPECT_EQ(fin, frame.stream_frame->fin);
  }

  // Returns the number of bytes consumed by the header of packet, including
  // the version.
  size_t GetPacketHeaderOverhead(InFecGroup is_in_fec_group) {
    return GetPacketHeaderSize(creator_.options()->send_connection_id_length,
                               kIncludeVersion,
                               creator_.options()->send_sequence_number_length,
                               is_in_fec_group);
  }

  // Returns the number of bytes of overhead that will be added to a packet
  // of maximum length.
  size_t GetEncryptionOverhead() {
    return creator_.options()->max_packet_length -
        client_framer_.GetMaxPlaintextSize(
            creator_.options()->max_packet_length);
  }

  // Returns the number of bytes consumed by the non-data fields of a stream
  // frame, assuming it is the last frame in the packet
  size_t GetStreamFrameOverhead(InFecGroup is_in_fec_group) {
    return QuicFramer::GetMinStreamFrameSize(
        client_framer_.version(), kStreamId, kOffset, true, is_in_fec_group);
  }

  static const QuicStreamId kStreamId = 1u;
  static const QuicStreamOffset kOffset = 1u;

  QuicFrames frames_;
  QuicFramer server_framer_;
  QuicFramer client_framer_;
  testing::StrictMock<MockFramerVisitor> framer_visitor_;
  QuicPacketSequenceNumber sequence_number_;
  QuicConnectionId connection_id_;
  string data_;
  MockRandom mock_random_;
  QuicPacketCreator creator_;
};

TEST_F(QuicPacketCreatorTest, SerializeFrames) {
  frames_.push_back(QuicFrame(new QuicAckFrame(MakeAckFrame(0u, 0u))));
  frames_.push_back(QuicFrame(new QuicStreamFrame(0u, false, 0u, IOVector())));
  frames_.push_back(QuicFrame(new QuicStreamFrame(0u, true, 0u, IOVector())));
  SerializedPacket serialized = creator_.SerializeAllFrames(frames_);
  delete frames_[0].ack_frame;
  delete frames_[1].stream_frame;
  delete frames_[2].stream_frame;

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
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

  frames_.push_back(QuicFrame(new QuicStreamFrame(0u, false, 0u, IOVector())));
  SerializedPacket serialized = creator_.SerializeAllFrames(frames_);
  delete frames_[0].stream_frame;

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
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
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnFecData(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;
}

TEST_F(QuicPacketCreatorTest, SerializeChangingSequenceNumberLength) {
  frames_.push_back(QuicFrame(new QuicAckFrame(MakeAckFrame(0u, 0u))));
  creator_.AddSavedFrame(frames_[0]);
  creator_.options()->send_sequence_number_length =
      PACKET_4BYTE_SEQUENCE_NUMBER;
  SerializedPacket serialized = creator_.SerializePacket();
  // The sequence number length will not change mid-packet.
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER, serialized.sequence_number_length);

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnAckFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;

  creator_.AddSavedFrame(frames_[0]);
  serialized = creator_.SerializePacket();
  // Now the actual sequence number length should have changed.
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER, serialized.sequence_number_length);
  delete frames_[0].ack_frame;

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnAckFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;
}

TEST_F(QuicPacketCreatorTest, SerializeWithFECChangingSequenceNumberLength) {
  creator_.options()->max_packets_per_fec_group = 6;
  ASSERT_FALSE(creator_.ShouldSendFec(false));

  frames_.push_back(QuicFrame(new QuicAckFrame(MakeAckFrame(0u, 0u))));
  creator_.AddSavedFrame(frames_[0]);
  // Change the sequence number length mid-FEC group and it should not change.
  creator_.options()->send_sequence_number_length =
      PACKET_4BYTE_SEQUENCE_NUMBER;
  SerializedPacket serialized = creator_.SerializePacket();
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER, serialized.sequence_number_length);

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnFecProtectedPayload(_));
    EXPECT_CALL(framer_visitor_, OnAckFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;

  ASSERT_FALSE(creator_.ShouldSendFec(false));
  ASSERT_TRUE(creator_.ShouldSendFec(true));

  serialized = creator_.SerializeFec();
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER, serialized.sequence_number_length);
  ASSERT_EQ(2u, serialized.sequence_number);

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnFecData(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;

  // Ensure the next FEC group starts using the new sequence number length.
  serialized = creator_.SerializeAllFrames(frames_);
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER, serialized.sequence_number_length);
  delete frames_[0].ack_frame;
  delete serialized.packet;
}

TEST_F(QuicPacketCreatorTest, ReserializeFramesWithSequenceNumberLength) {
  // If the original packet sequence number length, the current sequence number
  // length, and the configured send sequence number length are different, the
  // retransmit must sent with the original length and the others do not change.
  creator_.options()->send_sequence_number_length =
      PACKET_4BYTE_SEQUENCE_NUMBER;
  QuicPacketCreatorPeer::SetSequenceNumberLength(&creator_,
                                                 PACKET_2BYTE_SEQUENCE_NUMBER);
  frames_.push_back(QuicFrame(new QuicStreamFrame(0u, false, 0u, IOVector())));
  SerializedPacket serialized =
      creator_.ReserializeAllFrames(frames_, PACKET_1BYTE_SEQUENCE_NUMBER);
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER,
            creator_.options()->send_sequence_number_length);
  EXPECT_EQ(PACKET_2BYTE_SEQUENCE_NUMBER,
            QuicPacketCreatorPeer::GetSequenceNumberLength(&creator_));
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER, serialized.sequence_number_length);
  delete frames_[0].stream_frame;

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(serialized.packet);
  delete serialized.packet;
}

TEST_F(QuicPacketCreatorTest, SerializeConnectionClose) {
  QuicConnectionCloseFrame frame;
  frame.error_code = QUIC_NO_ERROR;
  frame.error_details = "error";

  SerializedPacket serialized = creator_.SerializeConnectionClose(&frame);
  ASSERT_EQ(1u, serialized.sequence_number);
  ASSERT_EQ(1u, creator_.sequence_number());

  InSequence s;
  EXPECT_CALL(framer_visitor_, OnPacket());
  EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
  EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnConnectionCloseFrame(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  ProcessPacket(serialized.packet);
  delete serialized.packet;
}

TEST_F(QuicPacketCreatorTest, CreateStreamFrame) {
  QuicFrame frame;
  size_t consumed = creator_.CreateStreamFrame(1u, MakeIOVector("test"), 0u,
                                               false, &frame);
  EXPECT_EQ(4u, consumed);
  CheckStreamFrame(frame, 1u, "test", 0u, false);
  delete frame.stream_frame;
}

TEST_F(QuicPacketCreatorTest, CreateStreamFrameFin) {
  QuicFrame frame;
  size_t consumed = creator_.CreateStreamFrame(1u, MakeIOVector("test"), 10u,
                                               true, &frame);
  EXPECT_EQ(4u, consumed);
  CheckStreamFrame(frame, 1u, "test", 10u, true);
  delete frame.stream_frame;
}

TEST_F(QuicPacketCreatorTest, CreateStreamFrameFinOnly) {
  QuicFrame frame;
  size_t consumed = creator_.CreateStreamFrame(1u, IOVector(), 0u, true,
                                               &frame);
  EXPECT_EQ(0u, consumed);
  CheckStreamFrame(frame, 1u, string(), 0u, true);
  delete frame.stream_frame;
}

TEST_F(QuicPacketCreatorTest, CreateAllFreeBytesForStreamFrames) {
  const size_t overhead = GetPacketHeaderOverhead(NOT_IN_FEC_GROUP)
                          + GetEncryptionOverhead();
  for (size_t i = overhead; i < overhead + 100; ++i) {
    creator_.options()->max_packet_length = i;
    const bool should_have_room = i > overhead + GetStreamFrameOverhead(
        NOT_IN_FEC_GROUP);
    ASSERT_EQ(should_have_room,
              creator_.HasRoomForStreamFrame(kStreamId, kOffset));
    if (should_have_room) {
      QuicFrame frame;
      size_t bytes_consumed = creator_.CreateStreamFrame(
          kStreamId, MakeIOVector("testdata"), kOffset, false, &frame);
      EXPECT_LT(0u, bytes_consumed);
      ASSERT_TRUE(creator_.AddSavedFrame(frame));
      SerializedPacket serialized_packet = creator_.SerializePacket();
      ASSERT_TRUE(serialized_packet.packet);
      delete serialized_packet.packet;
      delete serialized_packet.retransmittable_frames;
    }
  }
}

TEST_F(QuicPacketCreatorTest, StreamFrameConsumption) {
  // Compute the total overhead for a single frame in packet.
  const size_t overhead = GetPacketHeaderOverhead(NOT_IN_FEC_GROUP)
      + GetEncryptionOverhead() + GetStreamFrameOverhead(NOT_IN_FEC_GROUP);
  size_t capacity = kDefaultMaxPacketSize - overhead;
  // Now, test various sizes around this size.
  for (int delta = -5; delta <= 5; ++delta) {
    string data(capacity + delta, 'A');
    size_t bytes_free = delta > 0 ? 0 : 0 - delta;
    QuicFrame frame;
    size_t bytes_consumed = creator_.CreateStreamFrame(
        kStreamId, MakeIOVector(data), kOffset, false, &frame);
    EXPECT_EQ(capacity - bytes_free, bytes_consumed);

    ASSERT_TRUE(creator_.AddSavedFrame(frame));
    // BytesFree() returns bytes available for the next frame, which will
    // be two bytes smaller since the stream frame would need to be grown.
    EXPECT_EQ(2u, creator_.ExpansionOnNewFrame());
    size_t expected_bytes_free = bytes_free < 3 ? 0 : bytes_free - 2;
    EXPECT_EQ(expected_bytes_free, creator_.BytesFree()) << "delta: " << delta;
    SerializedPacket serialized_packet = creator_.SerializePacket();
    ASSERT_TRUE(serialized_packet.packet);
    delete serialized_packet.packet;
    delete serialized_packet.retransmittable_frames;
  }
}

TEST_F(QuicPacketCreatorTest, StreamFrameConsumptionInFecProtectedPacket) {
  // Turn on FEC protection.
  creator_.options()->max_packets_per_fec_group = 6;
  // Compute the total overhead for a single frame in packet.
  const size_t overhead = GetPacketHeaderOverhead(IN_FEC_GROUP)
      + GetEncryptionOverhead() + GetStreamFrameOverhead(IN_FEC_GROUP);
  size_t capacity = kDefaultMaxPacketSize - overhead;
  // Now, test various sizes around this size.
  for (int delta = -5; delta <= 5; ++delta) {
    string data(capacity + delta, 'A');
    size_t bytes_free = delta > 0 ? 0 : 0 - delta;
    QuicFrame frame;
    size_t bytes_consumed = creator_.CreateStreamFrame(
        kStreamId, MakeIOVector(data), kOffset, false, &frame);
    EXPECT_EQ(capacity - bytes_free, bytes_consumed);

    ASSERT_TRUE(creator_.AddSavedFrame(frame));
    // BytesFree() returns bytes available for the next frame. Since stream
    // frame does not grow for FEC protected packets, this should be the same
    // as bytes_free (bound by 0).
    EXPECT_EQ(0u, creator_.ExpansionOnNewFrame());
    size_t expected_bytes_free = bytes_free > 0 ? bytes_free : 0;
    EXPECT_EQ(expected_bytes_free, creator_.BytesFree()) << "delta: " << delta;
    SerializedPacket serialized_packet = creator_.SerializePacket();
    ASSERT_TRUE(serialized_packet.packet);
    delete serialized_packet.packet;
    delete serialized_packet.retransmittable_frames;
  }
}

TEST_F(QuicPacketCreatorTest, CryptoStreamFramePacketPadding) {
  // Compute the total overhead for a single frame in packet.
  const size_t overhead = GetPacketHeaderOverhead(NOT_IN_FEC_GROUP)
      + GetEncryptionOverhead() + GetStreamFrameOverhead(NOT_IN_FEC_GROUP);
  ASSERT_GT(kMaxPacketSize, overhead);
  size_t capacity = kDefaultMaxPacketSize - overhead;
  // Now, test various sizes around this size.
  for (int delta = -5; delta <= 5; ++delta) {
    string data(capacity + delta, 'A');
    size_t bytes_free = delta > 0 ? 0 : 0 - delta;

    QuicFrame frame;
    size_t bytes_consumed = creator_.CreateStreamFrame(
        kStreamId, MakeIOVector(data), kOffset, false, &frame);
    EXPECT_LT(0u, bytes_consumed);
    ASSERT_TRUE(creator_.AddSavedFrame(frame));
    SerializedPacket serialized_packet = creator_.SerializePacket();
    ASSERT_TRUE(serialized_packet.packet);
    // If there is not enough space in the packet to fit a padding frame
    // (1 byte) and to expand the stream frame (another 2 bytes) the packet
    // will not be padded.
    if (bytes_free < 3) {
      EXPECT_EQ(client_framer_.GetMaxPlaintextSize(kDefaultMaxPacketSize)
                - bytes_free, serialized_packet.packet->length());
    } else {
      EXPECT_EQ(client_framer_.GetMaxPlaintextSize(kDefaultMaxPacketSize),
                serialized_packet.packet->length());
    }
    delete serialized_packet.packet;
    delete serialized_packet.retransmittable_frames;
  }
}

TEST_F(QuicPacketCreatorTest, NonCryptoStreamFramePacketNonPadding) {
  // Compute the total overhead for a single frame in packet.
  const size_t overhead = GetPacketHeaderOverhead(NOT_IN_FEC_GROUP)
      + GetEncryptionOverhead() + GetStreamFrameOverhead(NOT_IN_FEC_GROUP);
  ASSERT_GT(kDefaultMaxPacketSize, overhead);
  size_t capacity = kDefaultMaxPacketSize - overhead;
  // Now, test various sizes around this size.
  for (int delta = -5; delta <= 5; ++delta) {
    string data(capacity + delta, 'A');
    size_t bytes_free = delta > 0 ? 0 : 0 - delta;

    QuicFrame frame;
    size_t bytes_consumed = creator_.CreateStreamFrame(
        kStreamId + 2, MakeIOVector(data), kOffset, false, &frame);
    EXPECT_LT(0u, bytes_consumed);
    ASSERT_TRUE(creator_.AddSavedFrame(frame));
    SerializedPacket serialized_packet = creator_.SerializePacket();
    ASSERT_TRUE(serialized_packet.packet);
    if (bytes_free > 0) {
      EXPECT_EQ(client_framer_.GetMaxPlaintextSize(kDefaultMaxPacketSize)
                - bytes_free, serialized_packet.packet->length());
    } else {
      EXPECT_EQ(client_framer_.GetMaxPlaintextSize(kDefaultMaxPacketSize),
                serialized_packet.packet->length());
    }
    delete serialized_packet.packet;
    delete serialized_packet.retransmittable_frames;
  }
}

TEST_F(QuicPacketCreatorTest, SerializeVersionNegotiationPacket) {
  QuicPacketCreatorPeer::SetIsServer(&creator_, true);
  QuicVersionVector versions;
  versions.push_back(test::QuicVersionMax());
  scoped_ptr<QuicEncryptedPacket> encrypted(
      creator_.SerializeVersionNegotiationPacket(versions));

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnVersionNegotiationPacket(_));
  }
  client_framer_.ProcessPacket(*encrypted.get());
}

TEST_F(QuicPacketCreatorTest, UpdatePacketSequenceNumberLengthLeastAwaiting) {
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER,
            creator_.options()->send_sequence_number_length);

  creator_.set_sequence_number(64);
  creator_.UpdateSequenceNumberLength(2, 10000);
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER,
            creator_.options()->send_sequence_number_length);

  creator_.set_sequence_number(64 * 256);
  creator_.UpdateSequenceNumberLength(2, 10000);
  EXPECT_EQ(PACKET_2BYTE_SEQUENCE_NUMBER,
            creator_.options()->send_sequence_number_length);

  creator_.set_sequence_number(64 * 256 * 256);
  creator_.UpdateSequenceNumberLength(2, 10000);
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER,
            creator_.options()->send_sequence_number_length);

  creator_.set_sequence_number(GG_UINT64_C(64) * 256 * 256 * 256 * 256);
  creator_.UpdateSequenceNumberLength(2, 10000);
  EXPECT_EQ(PACKET_6BYTE_SEQUENCE_NUMBER,
            creator_.options()->send_sequence_number_length);
}

TEST_F(QuicPacketCreatorTest, UpdatePacketSequenceNumberLengthBandwidth) {
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER,
            creator_.options()->send_sequence_number_length);

  creator_.UpdateSequenceNumberLength(1, 10000);
  EXPECT_EQ(PACKET_1BYTE_SEQUENCE_NUMBER,
            creator_.options()->send_sequence_number_length);

  creator_.UpdateSequenceNumberLength(1, 10000 * 256);
  EXPECT_EQ(PACKET_2BYTE_SEQUENCE_NUMBER,
            creator_.options()->send_sequence_number_length);

  creator_.UpdateSequenceNumberLength(1, 10000 * 256 * 256);
  EXPECT_EQ(PACKET_4BYTE_SEQUENCE_NUMBER,
            creator_.options()->send_sequence_number_length);

  creator_.UpdateSequenceNumberLength(
      1, GG_UINT64_C(1000) * 256 * 256 * 256 * 256);
  EXPECT_EQ(PACKET_6BYTE_SEQUENCE_NUMBER,
            creator_.options()->send_sequence_number_length);
}

TEST_F(QuicPacketCreatorTest, CreateStreamFrameWithNotifier) {
  // Ensure that if CreateStreamFrame does not consume any data (e.g. a FIN only
  // frame) then any QuicAckNotifier that is passed in still gets attached to
  // the frame.
  scoped_refptr<MockAckNotifierDelegate> delegate(new MockAckNotifierDelegate);
  QuicAckNotifier notifier(delegate.get());
  QuicFrame frame;
  IOVector empty_iovector;
  bool fin = true;
  size_t consumed_bytes = creator_.CreateStreamFrameWithNotifier(
      1u, empty_iovector, 0u, fin, &notifier, &frame);
  EXPECT_EQ(0u, consumed_bytes);
  EXPECT_EQ(&notifier, frame.stream_frame->notifier);
  delete frame.stream_frame;
}

INSTANTIATE_TEST_CASE_P(ToggleVersionSerialization,
                        QuicPacketCreatorTest,
                        ::testing::Values(false, true));

TEST_P(QuicPacketCreatorTest, SerializeFrame) {
  if (!GetParam()) {
    creator_.StopSendingVersion();
  }
  frames_.push_back(QuicFrame(new QuicStreamFrame(0u, false, 0u, IOVector())));
  SerializedPacket serialized = creator_.SerializeAllFrames(frames_);
  delete frames_[0].stream_frame;

  QuicPacketHeader header;
  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket());
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedPublicHeader(_));
    EXPECT_CALL(framer_visitor_, OnUnauthenticatedHeader(_));
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
  size_t payload_length;
  creator_.options()->max_packet_length = GetPacketLengthForOneStream(
      client_framer_.version(),
      QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
      PACKET_1BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP, &payload_length);
  QuicFrame frame;
  const string too_long_payload(payload_length * 2, 'a');
  size_t consumed = creator_.CreateStreamFrame(
      1u, MakeIOVector(too_long_payload), 0u, true, &frame);
  EXPECT_EQ(payload_length, consumed);
  const string payload(payload_length, 'a');
  CheckStreamFrame(frame, 1u, payload, 0u, false);
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
                creator_.options()->send_connection_id_length,
                QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
                PACKET_1BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
            creator_.BytesFree());

  // Add a variety of frame types and then a padding frame.
  QuicAckFrame ack_frame(MakeAckFrame(0u, 0u));
  EXPECT_TRUE(creator_.AddSavedFrame(QuicFrame(&ack_frame)));
  EXPECT_TRUE(creator_.HasPendingFrames());

  QuicCongestionFeedbackFrame congestion_feedback;
  congestion_feedback.type = kFixRate;
  EXPECT_TRUE(creator_.AddSavedFrame(QuicFrame(&congestion_feedback)));
  EXPECT_TRUE(creator_.HasPendingFrames());

  QuicFrame frame;
  size_t consumed = creator_.CreateStreamFrame(
      1u, MakeIOVector("test"), 0u, false, &frame);
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
                creator_.options()->send_connection_id_length,
                QuicPacketCreatorPeer::SendVersionInPacket(&creator_),
                PACKET_1BYTE_SEQUENCE_NUMBER,
                NOT_IN_FEC_GROUP),
            creator_.BytesFree());
}

TEST_F(QuicPacketCreatorTest, EntropyFlag) {
  frames_.push_back(QuicFrame(new QuicStreamFrame(0u, false, 0u, IOVector())));

  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 64; ++j) {
      SerializedPacket serialized = creator_.SerializeAllFrames(frames_);
      // Verify both BoolSource and hash algorithm.
      bool expected_rand_bool =
          (mock_random_.RandUint64() & (GG_UINT64_C(1) << j)) != 0;
      bool observed_rand_bool =
          (serialized.entropy_hash & (1 << ((j+1) % 8))) != 0;
      uint8 rest_of_hash = serialized.entropy_hash & ~(1 << ((j+1) % 8));
      EXPECT_EQ(expected_rand_bool, observed_rand_bool);
      EXPECT_EQ(0, rest_of_hash);
      delete serialized.packet;
    }
    // After 64 calls, BoolSource will refresh the bucket - make sure it does.
    mock_random_.ChangeValue();
  }

  delete frames_[0].stream_frame;
}

}  // namespace
}  // namespace test
}  // namespace net
