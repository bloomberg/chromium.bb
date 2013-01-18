// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_creator.h"

#include "base/stl_util.h"
#include "net/quic/crypto/null_encrypter.h"
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
        utils_(guid_, &framer_) {
    framer_.set_visitor(&framer_visitor_);
  }
  ~QuicPacketCreatorTest() {
    for (QuicFrames::iterator it = frames_.begin(); it != frames_.end(); ++it) {
      QuicConnection::DeleteEnclosedFrame(&(*it));
    }
  }

  void ProcessPacket(QuicPacket* packet) {
    scoped_ptr<QuicEncryptedPacket> encrypted(framer_.EncryptPacket(*packet));
    framer_.ProcessPacket(IPEndPoint(), IPEndPoint(), *encrypted);
  }

  QuicFrames frames_;
  QuicFramer framer_;
  testing::StrictMock<MockFramerVisitor> framer_visitor_;
  QuicStreamId id_;
  QuicPacketSequenceNumber sequence_number_;
  QuicGuid guid_;
  string data_;
  QuicPacketCreator utils_;
};

TEST_F(QuicPacketCreatorTest, SerializeFrame) {
  frames_.push_back(QuicFrame(new QuicStreamFrame(
      0u, false, 0u, StringPiece(""))));
  PacketPair pair = utils_.SerializeAllFrames(frames_);

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket(_, _));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(pair.second);
  delete pair.second;
}

TEST_F(QuicPacketCreatorTest, SerializeFrames) {
  frames_.push_back(QuicFrame(new QuicAckFrame(0u, 0u)));
  frames_.push_back(QuicFrame(new QuicStreamFrame(
      0u, false, 0u, StringPiece(""))));
  frames_.push_back(QuicFrame(new QuicStreamFrame(
      0u, true, 0u, StringPiece(""))));
  PacketPair pair = utils_.SerializeAllFrames(frames_);

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket(_, _));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnAckFrame(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(pair.second);
  delete pair.second;
}

TEST_F(QuicPacketCreatorTest, SerializeWithFEC) {
  utils_.options()->max_packets_per_fec_group = 6;
  utils_.MaybeStartFEC();

  frames_.push_back(QuicFrame(new QuicStreamFrame(
      0u, false, 0u, StringPiece(""))));
  PacketPair pair = utils_.SerializeAllFrames(frames_);

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket(_, _));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnFecProtectedPayload(_));
    EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(pair.second);
  delete pair.second;

  ASSERT_FALSE(utils_.ShouldSendFec(false));
  ASSERT_TRUE(utils_.ShouldSendFec(true));

  pair = utils_.SerializeFec();
  ASSERT_EQ(2u, pair.first);

  {
    InSequence s;
    EXPECT_CALL(framer_visitor_, OnPacket(_, _));
    EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
    EXPECT_CALL(framer_visitor_, OnFecData(_));
    EXPECT_CALL(framer_visitor_, OnPacketComplete());
  }
  ProcessPacket(pair.second);
  delete pair.second;
}

TEST_F(QuicPacketCreatorTest, CloseConnection) {
  QuicConnectionCloseFrame frame;
  frame.error_code = QUIC_NO_ERROR;
  frame.ack_frame = QuicAckFrame(0u, 0u);

  PacketPair pair = utils_.CloseConnection(&frame);
  ASSERT_EQ(1u, pair.first);
  ASSERT_EQ(1u, utils_.sequence_number());

  InSequence s;
  EXPECT_CALL(framer_visitor_, OnPacket(_, _));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnAckFrame(_));
  EXPECT_CALL(framer_visitor_, OnConnectionCloseFrame(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  ProcessPacket(pair.second);
  delete pair.second;
}

}  // namespace
}  // namespace test
}  // namespace net
