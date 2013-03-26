// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_test_utils.h"

#include "base/stl_util.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_packet_creator.h"

using std::max;
using std::min;
using std::string;
using testing::_;

namespace net {
namespace test {

MockFramerVisitor::MockFramerVisitor() {
  // By default, we want to accept packets.
  ON_CALL(*this, OnProtocolVersionMismatch(_))
      .WillByDefault(testing::Return(false));

  // By default, we want to accept packets.
  ON_CALL(*this, OnPacketHeader(_))
      .WillByDefault(testing::Return(true));
}

MockFramerVisitor::~MockFramerVisitor() {
}

bool NoOpFramerVisitor::OnProtocolVersionMismatch(QuicVersionTag version) {
  return false;
}

bool NoOpFramerVisitor::OnPacketHeader(const QuicPacketHeader& header) {
  return true;
}

FramerVisitorCapturingFrames::FramerVisitorCapturingFrames() : frame_count_(0) {
}

FramerVisitorCapturingFrames::~FramerVisitorCapturingFrames() {
}

bool FramerVisitorCapturingFrames::OnPacketHeader(
    const QuicPacketHeader& header) {
  header_ = header;
  frame_count_ = 0;
  return true;
}

void FramerVisitorCapturingFrames::OnStreamFrame(const QuicStreamFrame& frame) {
  // TODO(ianswett): Own the underlying string, so it will not exist outside
  // this callback.
  stream_frames_.push_back(frame);
  ++frame_count_;
}

void FramerVisitorCapturingFrames::OnAckFrame(const QuicAckFrame& frame) {
  ack_.reset(new QuicAckFrame(frame));
  ++frame_count_;
}

void FramerVisitorCapturingFrames::OnCongestionFeedbackFrame(
    const QuicCongestionFeedbackFrame& frame) {
  feedback_.reset(new QuicCongestionFeedbackFrame(frame));
  ++frame_count_;
}

void FramerVisitorCapturingFrames::OnRstStreamFrame(
    const QuicRstStreamFrame& frame) {
  rst_.reset(new QuicRstStreamFrame(frame));
  ++frame_count_;
}

void FramerVisitorCapturingFrames::OnConnectionCloseFrame(
    const QuicConnectionCloseFrame& frame) {
  close_.reset(new QuicConnectionCloseFrame(frame));
  ++frame_count_;
}

void FramerVisitorCapturingFrames::OnGoAwayFrame(const QuicGoAwayFrame& frame) {
  goaway_.reset(new QuicGoAwayFrame(frame));
  ++frame_count_;
}

void FramerVisitorCapturingFrames::OnVersionNegotiationPacket(
    const QuicVersionNegotiationPacket& packet) {
  version_negotiation_packet_.reset(new QuicVersionNegotiationPacket(packet));
  frame_count_ = 0;
}

FramerVisitorCapturingPublicReset::FramerVisitorCapturingPublicReset() {
}

FramerVisitorCapturingPublicReset::~FramerVisitorCapturingPublicReset() {
}

void FramerVisitorCapturingPublicReset::OnPublicResetPacket(
    const QuicPublicResetPacket& public_reset) {
  public_reset_packet_ = public_reset;
}

MockConnectionVisitor::MockConnectionVisitor() {
}

MockConnectionVisitor::~MockConnectionVisitor() {
}

MockHelper::MockHelper() {
}

MockHelper::~MockHelper() {
}

const QuicClock* MockHelper::GetClock() const {
  return &clock_;
}

QuicRandom* MockHelper::GetRandomGenerator() {
  return &random_generator_;
}

MockConnection::MockConnection(QuicGuid guid,
                               IPEndPoint address,
                               bool is_server)
    : QuicConnection(guid, address, new MockHelper(), is_server) {
}

MockConnection::MockConnection(QuicGuid guid,
                               IPEndPoint address,
                               QuicConnectionHelperInterface* helper,
                               bool is_server)
    : QuicConnection(guid, address, helper, is_server) {
}

MockConnection::~MockConnection() {
}

PacketSavingConnection::PacketSavingConnection(QuicGuid guid,
                                               IPEndPoint address,
                                               bool is_server)
    : MockConnection(guid, address, is_server) {
}

PacketSavingConnection::~PacketSavingConnection() {
  STLDeleteElements(&packets_);
}

bool PacketSavingConnection::SendOrQueuePacket(
    QuicPacketSequenceNumber sequence_number,
    QuicPacket* packet,
    QuicPacketEntropyHash entropy_hash,
    bool has_retransmittable_data) {
  packets_.push_back(packet);
  return true;
}

MockSession::MockSession(QuicConnection* connection, bool is_server)
    : QuicSession(connection, is_server) {
  ON_CALL(*this, WriteData(_, _, _, _))
      .WillByDefault(testing::Return(QuicConsumedData(0, false)));
}

MockSession::~MockSession() {
}

MockSendAlgorithm::MockSendAlgorithm() {
}

MockSendAlgorithm::~MockSendAlgorithm() {
}

namespace {

string HexDumpWithMarks(const char* data, int length,
                        const bool* marks, int mark_length) {
  static const char kHexChars[] = "0123456789abcdef";
  static const int kColumns = 4;

  const int kSizeLimit = 1024;
  if (length > kSizeLimit || mark_length > kSizeLimit) {
    LOG(ERROR) << "Only dumping first " << kSizeLimit << " bytes.";
    length = min(length, kSizeLimit);
    mark_length = min(mark_length, kSizeLimit);
  }

  string hex;
  for (const char* row = data; length > 0;
       row += kColumns, length -= kColumns) {
    for (const char *p = row; p < row + 4; ++p) {
      if (p < row + length) {
        const bool mark =
            (marks && (p - data) < mark_length && marks[p - data]);
        hex += mark ? '*' : ' ';
        hex += kHexChars[(*p & 0xf0) >> 4];
        hex += kHexChars[*p & 0x0f];
        hex += mark ? '*' : ' ';
      } else {
        hex += "    ";
      }
    }
    hex = hex + "  ";

    for (const char *p = row; p < row + 4 && p < row + length; ++p)
      hex += (*p >= 0x20 && *p <= 0x7f) ? (*p) : '.';

    hex = hex + '\n';
  }
  return hex;
}

}  // namespace

void CompareCharArraysWithHexError(
    const string& description,
    const char* actual,
    const int actual_len,
    const char* expected,
    const int expected_len) {
  const int min_len = min(actual_len, expected_len);
  const int max_len = max(actual_len, expected_len);
  scoped_array<bool> marks(new bool[max_len]);
  bool identical = (actual_len == expected_len);
  for (int i = 0; i < min_len; ++i) {
    if (actual[i] != expected[i]) {
      marks[i] = true;
      identical = false;
    } else {
      marks[i] = false;
    }
  }
  for (int i = min_len; i < max_len; ++i) {
    marks[i] = true;
  }
  if (identical) return;
  ADD_FAILURE()
      << "Description:\n"
      << description
      << "\n\nExpected:\n"
      << HexDumpWithMarks(expected, expected_len, marks.get(), max_len)
      << "\nActual:\n"
      << HexDumpWithMarks(actual, actual_len, marks.get(), max_len);
}

void CompareQuicDataWithHexError(
    const string& description,
    QuicData* actual,
    QuicData* expected) {
  CompareCharArraysWithHexError(
      description,
      actual->data(), actual->length(),
      expected->data(), expected->length());
}

static QuicPacket* ConstructPacketFromHandshakeMessage(
    QuicGuid guid,
    const CryptoHandshakeMessage& message,
    bool should_include_version) {
  CryptoFramer crypto_framer;
  scoped_ptr<QuicData> data(crypto_framer.ConstructHandshakeMessage(message));
  QuicFramer quic_framer(kQuicVersion1,
                         QuicDecrypter::Create(kNULL),
                         QuicEncrypter::Create(kNULL),
                         false);

  QuicPacketHeader header;
  header.public_header.guid = guid;
  header.public_header.reset_flag = false;
  header.public_header.version_flag = should_include_version;
  header.packet_sequence_number = 1;
  header.entropy_flag = false;
  header.entropy_hash = 0;
  header.fec_flag = false;
  header.fec_entropy_flag = false;
  header.fec_group = 0;

  QuicStreamFrame stream_frame(kCryptoStreamId, false, 0,
                               data->AsStringPiece());

  QuicFrame frame(&stream_frame);
  QuicFrames frames;
  frames.push_back(frame);
  return quic_framer.ConstructFrameDataPacket(header, frames).packet;
}

QuicPacket* ConstructHandshakePacket(QuicGuid guid, CryptoTag tag) {
  CryptoHandshakeMessage message;
  message.set_tag(tag);
  return ConstructPacketFromHandshakeMessage(guid, message, false);
}

size_t GetPacketLengthForOneStream(bool include_version, size_t payload) {
  // TODO(wtc): the hardcoded use of NullEncrypter here seems wrong.
  return NullEncrypter().GetCiphertextSize(payload) +
      QuicPacketCreator::StreamFramePacketOverhead(1, include_version);
}

QuicPacketEntropyHash TestEntropyCalculator::ReceivedEntropyHash(
    QuicPacketSequenceNumber sequence_number) const {
  return 1u;
}

}  // namespace test
}  // namespace net
