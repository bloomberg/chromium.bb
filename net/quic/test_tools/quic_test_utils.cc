// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/test_tools/quic_test_utils.h"

#include "base/sha1.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "net/quic/crypto/crypto_framer.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_data_writer.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_packet_creator.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/spdy/spdy_frame_builder.h"

using base::StringPiece;
using std::max;
using std::min;
using std::string;
using testing::AnyNumber;
using testing::_;

namespace net {
namespace test {
namespace {

// No-op alarm implementation used by MockHelper.
class TestAlarm : public QuicAlarm {
 public:
  explicit TestAlarm(QuicAlarm::Delegate* delegate)
      : QuicAlarm(delegate) {
  }

  void SetImpl() override {}
  void CancelImpl() override {}
};

}  // namespace

QuicAckFrame MakeAckFrame(QuicPacketSequenceNumber largest_observed) {
  QuicAckFrame ack;
  ack.largest_observed = largest_observed;
  ack.entropy_hash = 0;
  return ack;
}

QuicAckFrame MakeAckFrameWithNackRanges(
    size_t num_nack_ranges, QuicPacketSequenceNumber least_unacked) {
  QuicAckFrame ack = MakeAckFrame(2 * num_nack_ranges + least_unacked);
  // Add enough missing packets to get num_nack_ranges nack ranges.
  for (QuicPacketSequenceNumber i = 1; i < 2 * num_nack_ranges; i += 2) {
    ack.missing_packets.insert(least_unacked + i);
  }
  return ack;
}

QuicPacket* BuildUnsizedDataPacket(QuicFramer* framer,
                                   const QuicPacketHeader& header,
                                   const QuicFrames& frames) {
  const size_t max_plaintext_size = framer->GetMaxPlaintextSize(kMaxPacketSize);
  size_t packet_size = GetPacketHeaderSize(header);
  for (size_t i = 0; i < frames.size(); ++i) {
    DCHECK_LE(packet_size, max_plaintext_size);
    bool first_frame = i == 0;
    bool last_frame = i == frames.size() - 1;
    const size_t frame_size = framer->GetSerializedFrameLength(
        frames[i], max_plaintext_size - packet_size, first_frame, last_frame,
        header.is_in_fec_group,
        header.public_header.sequence_number_length);
    DCHECK(frame_size);
    packet_size += frame_size;
  }
  return BuildUnsizedDataPacket(framer, header, frames, packet_size);
}

QuicPacket* BuildUnsizedDataPacket(QuicFramer* framer,
                                   const QuicPacketHeader& header,
                                   const QuicFrames& frames,
                                   size_t packet_size) {
  char* buffer = new char[packet_size];
  scoped_ptr<QuicPacket> packet(
      framer->BuildDataPacket(header, frames, buffer, packet_size));
  DCHECK(packet.get() != nullptr);
  // Now I have to re-construct the data packet with data ownership.
  return new QuicPacket(buffer, packet->length(), true,
                        header.public_header.connection_id_length,
                        header.public_header.version_flag,
                        header.public_header.sequence_number_length);
}

uint64 SimpleRandom::RandUint64() {
  unsigned char hash[base::kSHA1Length];
  base::SHA1HashBytes(reinterpret_cast<unsigned char*>(&seed_), sizeof(seed_),
                      hash);
  memcpy(&seed_, hash, sizeof(seed_));
  return seed_;
}

MockFramerVisitor::MockFramerVisitor() {
  // By default, we want to accept packets.
  ON_CALL(*this, OnProtocolVersionMismatch(_))
      .WillByDefault(testing::Return(false));

  // By default, we want to accept packets.
  ON_CALL(*this, OnUnauthenticatedHeader(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnUnauthenticatedPublicHeader(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnPacketHeader(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnStreamFrame(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnAckFrame(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnStopWaitingFrame(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnPingFrame(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnRstStreamFrame(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnConnectionCloseFrame(_))
      .WillByDefault(testing::Return(true));

  ON_CALL(*this, OnGoAwayFrame(_))
      .WillByDefault(testing::Return(true));
}

MockFramerVisitor::~MockFramerVisitor() {
}

bool NoOpFramerVisitor::OnProtocolVersionMismatch(QuicVersion version) {
  return false;
}

bool NoOpFramerVisitor::OnUnauthenticatedPublicHeader(
    const QuicPacketPublicHeader& header) {
  return true;
}

bool NoOpFramerVisitor::OnUnauthenticatedHeader(
    const QuicPacketHeader& header) {
  return true;
}

bool NoOpFramerVisitor::OnPacketHeader(const QuicPacketHeader& header) {
  return true;
}

bool NoOpFramerVisitor::OnStreamFrame(const QuicStreamFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnAckFrame(const QuicAckFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnStopWaitingFrame(
    const QuicStopWaitingFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnPingFrame(const QuicPingFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnRstStreamFrame(
    const QuicRstStreamFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnConnectionCloseFrame(
    const QuicConnectionCloseFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnGoAwayFrame(const QuicGoAwayFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnWindowUpdateFrame(
    const QuicWindowUpdateFrame& frame) {
  return true;
}

bool NoOpFramerVisitor::OnBlockedFrame(const QuicBlockedFrame& frame) {
  return true;
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

QuicAlarm* MockHelper::CreateAlarm(QuicAlarm::Delegate* delegate) {
  return new TestAlarm(delegate);
}

void MockHelper::AdvanceTime(QuicTime::Delta delta) {
  clock_.AdvanceTime(delta);
}

QuicPacketWriter* NiceMockPacketWriterFactory::Create(
    QuicConnection* /*connection*/) const {
  return new testing::NiceMock<MockPacketWriter>();
}

MockConnection::MockConnection(Perspective perspective)
    : QuicConnection(kTestConnectionId,
                     IPEndPoint(TestPeerIPAddress(), kTestPort),
                     new testing::NiceMock<MockHelper>(),
                     NiceMockPacketWriterFactory(),
                     /* owns_writer= */ true,
                     perspective,
                     /* is_secure= */ false,
                     QuicSupportedVersions()),
      helper_(helper()) {
}

MockConnection::MockConnection(Perspective perspective, bool is_secure)
    : QuicConnection(kTestConnectionId,
                     IPEndPoint(TestPeerIPAddress(), kTestPort),
                     new testing::NiceMock<MockHelper>(),
                     NiceMockPacketWriterFactory(),
                     /* owns_writer= */ true,
                     perspective,
                     is_secure,
                     QuicSupportedVersions()),
      helper_(helper()) {
}

MockConnection::MockConnection(IPEndPoint address, Perspective perspective)
    : QuicConnection(kTestConnectionId,
                     address,
                     new testing::NiceMock<MockHelper>(),
                     NiceMockPacketWriterFactory(),
                     /* owns_writer= */ true,
                     perspective,
                     /* is_secure= */ false,
                     QuicSupportedVersions()),
      helper_(helper()) {
}

MockConnection::MockConnection(QuicConnectionId connection_id,
                               Perspective perspective)
    : QuicConnection(connection_id,
                     IPEndPoint(TestPeerIPAddress(), kTestPort),
                     new testing::NiceMock<MockHelper>(),
                     NiceMockPacketWriterFactory(),
                     /* owns_writer= */ true,
                     perspective,
                     /* is_secure= */ false,
                     QuicSupportedVersions()),
      helper_(helper()) {
}

MockConnection::MockConnection(QuicConnectionId connection_id,
                               Perspective perspective,
                               bool is_secure)
    : QuicConnection(connection_id,
                     IPEndPoint(TestPeerIPAddress(), kTestPort),
                     new testing::NiceMock<MockHelper>(),
                     NiceMockPacketWriterFactory(),
                     /* owns_writer= */ true,
                     perspective,
                     is_secure,
                     QuicSupportedVersions()),
      helper_(helper()) {
}

MockConnection::MockConnection(Perspective perspective,
                               const QuicVersionVector& supported_versions)
    : QuicConnection(kTestConnectionId,
                     IPEndPoint(TestPeerIPAddress(), kTestPort),
                     new testing::NiceMock<MockHelper>(),
                     NiceMockPacketWriterFactory(),
                     /* owns_writer= */ true,
                     perspective,
                     /* is_secure= */ false,
                     supported_versions),
      helper_(helper()) {
}

MockConnection::~MockConnection() {
}

void MockConnection::AdvanceTime(QuicTime::Delta delta) {
  static_cast<MockHelper*>(helper())->AdvanceTime(delta);
}

PacketSavingConnection::PacketSavingConnection(Perspective perspective)
    : MockConnection(perspective) {
}

PacketSavingConnection::PacketSavingConnection(
    Perspective perspective,
    const QuicVersionVector& supported_versions)
    : MockConnection(perspective, supported_versions) {
}

PacketSavingConnection::~PacketSavingConnection() {
  STLDeleteElements(&encrypted_packets_);
}

void PacketSavingConnection::SendOrQueuePacket(QueuedPacket packet) {
  encrypted_packets_.push_back(packet.serialized_packet.packet);
  // Transfer ownership of the packet to the SentPacketManager and the
  // ack notifier to the AckNotifierManager.
  sent_packet_manager_.OnPacketSent(
      &packet.serialized_packet, 0, QuicTime::Zero(), 1000,
      NOT_RETRANSMISSION, HAS_RETRANSMITTABLE_DATA);
}

MockSession::MockSession(QuicConnection* connection)
    : QuicSession(connection, DefaultQuicConfig()) {
  InitializeSession();
  ON_CALL(*this, WritevData(_, _, _, _, _, _))
      .WillByDefault(testing::Return(QuicConsumedData(0, false)));
}

MockSession::~MockSession() {
}

TestSession::TestSession(QuicConnection* connection, const QuicConfig& config)
    : QuicSession(connection, config),
      crypto_stream_(nullptr) {
  InitializeSession();
}

TestSession::~TestSession() {}

void TestSession::SetCryptoStream(QuicCryptoStream* stream) {
  crypto_stream_ = stream;
}

QuicCryptoStream* TestSession::GetCryptoStream() {
  return crypto_stream_;
}

TestClientSession::TestClientSession(QuicConnection* connection,
                                     const QuicConfig& config)
    : QuicClientSessionBase(connection, config),
      crypto_stream_(nullptr) {
  EXPECT_CALL(*this, OnProofValid(_)).Times(AnyNumber());
  InitializeSession();
}

TestClientSession::~TestClientSession() {}

void TestClientSession::SetCryptoStream(QuicCryptoStream* stream) {
  crypto_stream_ = stream;
}

QuicCryptoStream* TestClientSession::GetCryptoStream() {
  return crypto_stream_;
}

MockPacketWriter::MockPacketWriter() {
}

MockPacketWriter::~MockPacketWriter() {
}

MockSendAlgorithm::MockSendAlgorithm() {
}

MockSendAlgorithm::~MockSendAlgorithm() {
}

MockLossAlgorithm::MockLossAlgorithm() {
}

MockLossAlgorithm::~MockLossAlgorithm() {
}

MockAckNotifierDelegate::MockAckNotifierDelegate() {
}

MockAckNotifierDelegate::~MockAckNotifierDelegate() {
}

MockNetworkChangeVisitor::MockNetworkChangeVisitor() {
}

MockNetworkChangeVisitor::~MockNetworkChangeVisitor() {
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

    for (const char* p = row; p < row + 4 && p < row + length; ++p) {
      hex += (*p >= 0x20 && *p <= 0x7f) ? (*p) : '.';
    }

    hex = hex + '\n';
  }
  return hex;
}

}  // namespace

IPAddressNumber TestPeerIPAddress() { return Loopback4(); }

QuicVersion QuicVersionMax() { return QuicSupportedVersions().front(); }

QuicVersion QuicVersionMin() { return QuicSupportedVersions().back(); }

IPAddressNumber Loopback4() {
  IPAddressNumber addr;
  CHECK(ParseIPLiteralToNumber("127.0.0.1", &addr));
  return addr;
}

IPAddressNumber Loopback6() {
  IPAddressNumber addr;
  CHECK(ParseIPLiteralToNumber("::1", &addr));
  return addr;
}

void GenerateBody(string* body, int length) {
  body->clear();
  body->reserve(length);
  for (int i = 0; i < length; ++i) {
    body->append(1, static_cast<char>(32 + i % (126 - 32)));
  }
}

QuicEncryptedPacket* ConstructEncryptedPacket(
    QuicConnectionId connection_id,
    bool version_flag,
    bool reset_flag,
    QuicPacketSequenceNumber sequence_number,
    const string& data) {
  return ConstructEncryptedPacket(
      connection_id, version_flag, reset_flag, sequence_number, data,
      PACKET_8BYTE_CONNECTION_ID, PACKET_6BYTE_SEQUENCE_NUMBER);
}

QuicEncryptedPacket* ConstructEncryptedPacket(
    QuicConnectionId connection_id,
    bool version_flag,
    bool reset_flag,
    QuicPacketSequenceNumber sequence_number,
    const string& data,
    QuicConnectionIdLength connection_id_length,
    QuicSequenceNumberLength sequence_number_length) {
  QuicPacketHeader header;
  header.public_header.connection_id = connection_id;
  header.public_header.connection_id_length = connection_id_length;
  header.public_header.version_flag = version_flag;
  header.public_header.reset_flag = reset_flag;
  header.public_header.sequence_number_length = sequence_number_length;
  header.packet_sequence_number = sequence_number;
  header.entropy_flag = false;
  header.entropy_hash = 0;
  header.fec_flag = false;
  header.is_in_fec_group = NOT_IN_FEC_GROUP;
  header.fec_group = 0;
  QuicStreamFrame stream_frame(1, false, 0, MakeIOVector(data));
  QuicFrame frame(&stream_frame);
  QuicFrames frames;
  frames.push_back(frame);
  QuicFramer framer(QuicSupportedVersions(), QuicTime::Zero(),
                    Perspective::IS_CLIENT);
  scoped_ptr<QuicPacket> packet(
      BuildUnsizedDataPacket(&framer, header, frames));
  EXPECT_TRUE(packet != nullptr);
  QuicEncryptedPacket* encrypted = framer.EncryptPacket(ENCRYPTION_NONE,
                                                        sequence_number,
                                                        *packet);
  EXPECT_TRUE(encrypted != nullptr);
  return encrypted;
}

void CompareCharArraysWithHexError(
    const string& description,
    const char* actual,
    const int actual_len,
    const char* expected,
    const int expected_len) {
  EXPECT_EQ(actual_len, expected_len);
  const int min_len = min(actual_len, expected_len);
  const int max_len = max(actual_len, expected_len);
  scoped_ptr<bool[]> marks(new bool[max_len]);
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

bool DecodeHexString(const base::StringPiece& hex, std::string* bytes) {
  bytes->clear();
  if (hex.empty())
    return true;
  std::vector<uint8> v;
  if (!base::HexStringToBytes(hex.as_string(), &v))
    return false;
  if (!v.empty())
    bytes->assign(reinterpret_cast<const char*>(&v[0]), v.size());
  return true;
}

static QuicPacket* ConstructPacketFromHandshakeMessage(
    QuicConnectionId connection_id,
    const CryptoHandshakeMessage& message,
    bool should_include_version) {
  CryptoFramer crypto_framer;
  scoped_ptr<QuicData> data(crypto_framer.ConstructHandshakeMessage(message));
  QuicFramer quic_framer(QuicSupportedVersions(), QuicTime::Zero(),
                         Perspective::IS_CLIENT);

  QuicPacketHeader header;
  header.public_header.connection_id = connection_id;
  header.public_header.reset_flag = false;
  header.public_header.version_flag = should_include_version;
  header.packet_sequence_number = 1;
  header.entropy_flag = false;
  header.entropy_hash = 0;
  header.fec_flag = false;
  header.fec_group = 0;

  QuicStreamFrame stream_frame(kCryptoStreamId, false, 0,
                               MakeIOVector(data->AsStringPiece()));

  QuicFrame frame(&stream_frame);
  QuicFrames frames;
  frames.push_back(frame);
  return BuildUnsizedDataPacket(&quic_framer, header, frames);
}

QuicPacket* ConstructHandshakePacket(QuicConnectionId connection_id,
                                     QuicTag tag) {
  CryptoHandshakeMessage message;
  message.set_tag(tag);
  return ConstructPacketFromHandshakeMessage(connection_id, message, false);
}

size_t GetPacketLengthForOneStream(
    QuicVersion version,
    bool include_version,
    QuicConnectionIdLength connection_id_length,
    QuicSequenceNumberLength sequence_number_length,
    InFecGroup is_in_fec_group,
    size_t* payload_length) {
  *payload_length = 1;
  const size_t stream_length =
      NullEncrypter().GetCiphertextSize(*payload_length) +
      QuicPacketCreator::StreamFramePacketOverhead(
          PACKET_8BYTE_CONNECTION_ID, include_version,
          sequence_number_length, 0u, is_in_fec_group);
  const size_t ack_length = NullEncrypter().GetCiphertextSize(
      QuicFramer::GetMinAckFrameSize(
          sequence_number_length, PACKET_1BYTE_SEQUENCE_NUMBER)) +
      GetPacketHeaderSize(connection_id_length, include_version,
                          sequence_number_length, is_in_fec_group);
  if (stream_length < ack_length) {
    *payload_length = 1 + ack_length - stream_length;
  }

  return NullEncrypter().GetCiphertextSize(*payload_length) +
      QuicPacketCreator::StreamFramePacketOverhead(
          connection_id_length, include_version,
          sequence_number_length, 0u, is_in_fec_group);
}

TestEntropyCalculator::TestEntropyCalculator() {}

TestEntropyCalculator::~TestEntropyCalculator() {}

QuicPacketEntropyHash TestEntropyCalculator::EntropyHash(
    QuicPacketSequenceNumber sequence_number) const {
  return 1u;
}

MockEntropyCalculator::MockEntropyCalculator() {}

MockEntropyCalculator::~MockEntropyCalculator() {}

QuicConfig DefaultQuicConfig() {
  QuicConfig config;
  config.SetInitialStreamFlowControlWindowToSend(
      kInitialStreamFlowControlWindowForTest);
  config.SetInitialSessionFlowControlWindowToSend(
      kInitialSessionFlowControlWindowForTest);
  return config;
}

QuicVersionVector SupportedVersions(QuicVersion version) {
  QuicVersionVector versions;
  versions.push_back(version);
  return versions;
}

TestWriterFactory::TestWriterFactory() : current_writer_(nullptr) {}
TestWriterFactory::~TestWriterFactory() {}

QuicPacketWriter* TestWriterFactory::Create(QuicServerPacketWriter* writer,
                                            QuicConnection* connection) {
  return new PerConnectionPacketWriter(this, writer, connection);
}

void TestWriterFactory::OnPacketSent(WriteResult result) {
  if (current_writer_ != nullptr && result.status == WRITE_STATUS_ERROR) {
    current_writer_->connection()->OnWriteError(result.error_code);
    current_writer_ = nullptr;
  }
}

void TestWriterFactory::Unregister(PerConnectionPacketWriter* writer) {
  if (current_writer_ == writer) {
    current_writer_ = nullptr;
  }
}

TestWriterFactory::PerConnectionPacketWriter::PerConnectionPacketWriter(
    TestWriterFactory* factory,
    QuicServerPacketWriter* writer,
    QuicConnection* connection)
    : QuicPerConnectionPacketWriter(writer, connection),
      factory_(factory) {
}

TestWriterFactory::PerConnectionPacketWriter::~PerConnectionPacketWriter() {
  factory_->Unregister(this);
}

WriteResult TestWriterFactory::PerConnectionPacketWriter::WritePacket(
    const char* buffer,
    size_t buf_len,
    const IPAddressNumber& self_address,
    const IPEndPoint& peer_address) {
  // A DCHECK(factory_current_writer_ == nullptr) would be wrong here -- this
  // class may be used in a setting where connection()->OnPacketSent() is called
  // in a different way, so TestWriterFactory::OnPacketSent might never be
  // called.
  factory_->current_writer_ = this;
  return QuicPerConnectionPacketWriter::WritePacket(buffer,
                                                    buf_len,
                                                    self_address,
                                                    peer_address);
}

}  // namespace test
}  // namespace net
