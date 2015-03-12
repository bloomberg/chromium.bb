// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Common utilities for Quic tests

#ifndef NET_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_
#define NET_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string_piece.h"
#include "net/quic/congestion_control/loss_detection_interface.h"
#include "net/quic/congestion_control/send_algorithm_interface.h"
#include "net/quic/quic_ack_notifier.h"
#include "net/quic/quic_client_session_base.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_dispatcher.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_per_connection_packet_writer.h"
#include "net/quic/quic_sent_packet_manager.h"
#include "net/quic/quic_session.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/spdy/spdy_framer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

namespace test {

static const QuicConnectionId kTestConnectionId = 42;
static const uint16 kTestPort = 123;
static const uint32 kInitialStreamFlowControlWindowForTest =
    1024 * 1024;  // 1 MB
static const uint32 kInitialSessionFlowControlWindowForTest =
    1536 * 1024;  // 1.5 MB
// Data stream IDs start at 5: the crypto stream is 1, headers stream is 3.
static const QuicStreamId kClientDataStreamId1 = 5;
static const QuicStreamId kClientDataStreamId2 = 7;
static const QuicStreamId kClientDataStreamId3 = 9;

// Returns the test peer IP address.
IPAddressNumber TestPeerIPAddress();

// Upper limit on versions we support.
QuicVersion QuicVersionMax();

// Lower limit on versions we support.
QuicVersion QuicVersionMin();

// Returns an address for 127.0.0.1.
IPAddressNumber Loopback4();

// Returns an address for ::1.
IPAddressNumber Loopback6();

void GenerateBody(std::string* body, int length);

// Create an encrypted packet for testing.
QuicEncryptedPacket* ConstructEncryptedPacket(
    QuicConnectionId connection_id,
    bool version_flag,
    bool reset_flag,
    QuicPacketSequenceNumber sequence_number,
    const std::string& data,
    QuicConnectionIdLength connection_id_length,
    QuicSequenceNumberLength sequence_number_length);

// This form assumes |connection_id_length| == PACKET_8BYTE_CONNECTION_ID and
// |sequence_number_length| == PACKET_6BYTE_SEQUENCE_NUMBER.
QuicEncryptedPacket* ConstructEncryptedPacket(
    QuicConnectionId connection_id,
    bool version_flag,
    bool reset_flag,
    QuicPacketSequenceNumber sequence_number,
    const std::string& data);

void CompareCharArraysWithHexError(const std::string& description,
                                   const char* actual,
                                   const int actual_len,
                                   const char* expected,
                                   const int expected_len);

bool DecodeHexString(const base::StringPiece& hex, std::string* bytes);

// Returns the length of a QuicPacket that is capable of holding either a
// stream frame or a minimal ack frame.  Sets |*payload_length| to the number
// of bytes of stream data that will fit in such a packet.
size_t GetPacketLengthForOneStream(
    QuicVersion version,
    bool include_version,
    QuicConnectionIdLength connection_id_length,
    QuicSequenceNumberLength sequence_number_length,
    InFecGroup is_in_fec_group,
    size_t* payload_length);

// Returns QuicConfig set to default values.
QuicConfig DefaultQuicConfig();

// Returns a version vector consisting of |version|.
QuicVersionVector SupportedVersions(QuicVersion version);

// Testing convenience method to construct a QuicAckFrame with entropy_hash set
// to 0 and largest_observed from peer set to |largest_observed|.
QuicAckFrame MakeAckFrame(QuicPacketSequenceNumber largest_observed);

// Testing convenience method to construct a QuicAckFrame with |num_nack_ranges|
// nack ranges of width 1 packet, starting from |least_unacked|.
QuicAckFrame MakeAckFrameWithNackRanges(size_t num_nack_ranges,
                                        QuicPacketSequenceNumber least_unacked);

// Returns a QuicPacket that is owned by the caller, and
// is populated with the fields in |header| and |frames|, or is nullptr if the
// packet could not be created.
QuicPacket* BuildUnsizedDataPacket(QuicFramer* framer,
                                   const QuicPacketHeader& header,
                                   const QuicFrames& frames);
// Returns a QuicPacket that is owned by the caller, and of size |packet_size|.
QuicPacket* BuildUnsizedDataPacket(QuicFramer* framer,
                                   const QuicPacketHeader& header,
                                   const QuicFrames& frames,
                                   size_t packet_size);

template<typename SaveType>
class ValueRestore {
 public:
  ValueRestore(SaveType* name, SaveType value)
      : name_(name),
        value_(*name) {
    *name_ = value;
  }
  ~ValueRestore() {
    *name_ = value_;
  }

 private:
  SaveType* name_;
  SaveType value_;

  DISALLOW_COPY_AND_ASSIGN(ValueRestore);
};

// Simple random number generator used to compute random numbers suitable
// for pseudo-randomly dropping packets in tests.  It works by computing
// the sha1 hash of the current seed, and using the first 64 bits as
// the next random number, and the next seed.
class SimpleRandom {
 public:
  SimpleRandom() : seed_(0) {}

  // Returns a random number in the range [0, kuint64max].
  uint64 RandUint64();

  void set_seed(uint64 seed) { seed_ = seed; }

 private:
  uint64 seed_;

  DISALLOW_COPY_AND_ASSIGN(SimpleRandom);
};

class MockFramerVisitor : public QuicFramerVisitorInterface {
 public:
  MockFramerVisitor();
  ~MockFramerVisitor() override;

  MOCK_METHOD1(OnError, void(QuicFramer* framer));
  // The constructor sets this up to return false by default.
  MOCK_METHOD1(OnProtocolVersionMismatch, bool(QuicVersion version));
  MOCK_METHOD0(OnPacket, void());
  MOCK_METHOD1(OnPublicResetPacket, void(const QuicPublicResetPacket& header));
  MOCK_METHOD1(OnVersionNegotiationPacket,
               void(const QuicVersionNegotiationPacket& packet));
  MOCK_METHOD0(OnRevivedPacket, void());
  // The constructor sets this up to return true by default.
  MOCK_METHOD1(OnUnauthenticatedHeader, bool(const QuicPacketHeader& header));
  // The constructor sets this up to return true by default.
  MOCK_METHOD1(OnUnauthenticatedPublicHeader, bool(
      const QuicPacketPublicHeader& header));
  MOCK_METHOD1(OnDecryptedPacket, void(EncryptionLevel level));
  MOCK_METHOD1(OnPacketHeader, bool(const QuicPacketHeader& header));
  MOCK_METHOD1(OnFecProtectedPayload, void(base::StringPiece payload));
  MOCK_METHOD1(OnStreamFrame, bool(const QuicStreamFrame& frame));
  MOCK_METHOD1(OnAckFrame, bool(const QuicAckFrame& frame));
  MOCK_METHOD1(OnStopWaitingFrame, bool(const QuicStopWaitingFrame& frame));
  MOCK_METHOD1(OnPingFrame, bool(const QuicPingFrame& frame));
  MOCK_METHOD1(OnFecData, void(const QuicFecData& fec));
  MOCK_METHOD1(OnRstStreamFrame, bool(const QuicRstStreamFrame& frame));
  MOCK_METHOD1(OnConnectionCloseFrame,
               bool(const QuicConnectionCloseFrame& frame));
  MOCK_METHOD1(OnGoAwayFrame, bool(const QuicGoAwayFrame& frame));
  MOCK_METHOD1(OnWindowUpdateFrame, bool(const QuicWindowUpdateFrame& frame));
  MOCK_METHOD1(OnBlockedFrame, bool(const QuicBlockedFrame& frame));
  MOCK_METHOD0(OnPacketComplete, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFramerVisitor);
};

class NoOpFramerVisitor : public QuicFramerVisitorInterface {
 public:
  NoOpFramerVisitor() {}

  void OnError(QuicFramer* framer) override {}
  void OnPacket() override {}
  void OnPublicResetPacket(const QuicPublicResetPacket& packet) override {}
  void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) override {}
  void OnRevivedPacket() override {}
  bool OnProtocolVersionMismatch(QuicVersion version) override;
  bool OnUnauthenticatedHeader(const QuicPacketHeader& header) override;
  bool OnUnauthenticatedPublicHeader(
      const QuicPacketPublicHeader& header) override;
  void OnDecryptedPacket(EncryptionLevel level) override {}
  bool OnPacketHeader(const QuicPacketHeader& header) override;
  void OnFecProtectedPayload(base::StringPiece payload) override {}
  bool OnStreamFrame(const QuicStreamFrame& frame) override;
  bool OnAckFrame(const QuicAckFrame& frame) override;
  bool OnStopWaitingFrame(const QuicStopWaitingFrame& frame) override;
  bool OnPingFrame(const QuicPingFrame& frame) override;
  void OnFecData(const QuicFecData& fec) override {}
  bool OnRstStreamFrame(const QuicRstStreamFrame& frame) override;
  bool OnConnectionCloseFrame(const QuicConnectionCloseFrame& frame) override;
  bool OnGoAwayFrame(const QuicGoAwayFrame& frame) override;
  bool OnWindowUpdateFrame(const QuicWindowUpdateFrame& frame) override;
  bool OnBlockedFrame(const QuicBlockedFrame& frame) override;
  void OnPacketComplete() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NoOpFramerVisitor);
};

class MockConnectionVisitor : public QuicConnectionVisitorInterface {
 public:
  MockConnectionVisitor();
  ~MockConnectionVisitor() override;

  MOCK_METHOD1(OnStreamFrames, void(const std::vector<QuicStreamFrame>& frame));
  MOCK_METHOD1(OnWindowUpdateFrames,
               void(const std::vector<QuicWindowUpdateFrame>& frame));
  MOCK_METHOD1(OnBlockedFrames,
               void(const std::vector<QuicBlockedFrame>& frame));
  MOCK_METHOD1(OnRstStream, void(const QuicRstStreamFrame& frame));
  MOCK_METHOD1(OnGoAway, void(const QuicGoAwayFrame& frame));
  MOCK_METHOD2(OnConnectionClosed, void(QuicErrorCode error, bool from_peer));
  MOCK_METHOD0(OnWriteBlocked, void());
  MOCK_METHOD0(OnCanWrite, void());
  MOCK_METHOD1(OnCongestionWindowChange, void(QuicTime now));
  MOCK_CONST_METHOD0(WillingAndAbleToWrite, bool());
  MOCK_CONST_METHOD0(HasPendingHandshake, bool());
  MOCK_CONST_METHOD0(HasOpenDataStreams, bool());
  MOCK_METHOD1(OnSuccessfulVersionNegotiation,
               void(const QuicVersion& version));
  MOCK_METHOD0(OnConfigNegotiated, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockConnectionVisitor);
};

class MockHelper : public QuicConnectionHelperInterface {
 public:
  MockHelper();
  ~MockHelper() override;
  const QuicClock* GetClock() const override;
  QuicRandom* GetRandomGenerator() override;
  QuicAlarm* CreateAlarm(QuicAlarm::Delegate* delegate) override;
  void AdvanceTime(QuicTime::Delta delta);

 private:
  MockClock clock_;
  MockRandom random_generator_;

  DISALLOW_COPY_AND_ASSIGN(MockHelper);
};

class NiceMockPacketWriterFactory : public QuicConnection::PacketWriterFactory {
 public:
  NiceMockPacketWriterFactory() {}
  ~NiceMockPacketWriterFactory() override {}

  QuicPacketWriter* Create(QuicConnection* /*connection*/) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NiceMockPacketWriterFactory);
};

class MockConnection : public QuicConnection {
 public:
  // Uses a MockHelper, ConnectionId of 42, and 127.0.0.1:123.
  explicit MockConnection(bool is_server);

  // Uses a MockHelper, ConnectionId of 42, and 127.0.0.1:123.
  MockConnection(bool is_server, bool is_secure);

  // Uses a MockHelper, ConnectionId of 42.
  MockConnection(IPEndPoint address, bool is_server);

  // Uses a MockHelper, and 127.0.0.1:123
  MockConnection(QuicConnectionId connection_id, bool is_server);

  // Uses a Mock helper, ConnectionId of 42, and 127.0.0.1:123.
  MockConnection(bool is_server, const QuicVersionVector& supported_versions);

  ~MockConnection() override;

  // If the constructor that uses a MockHelper has been used then this method
  // will advance the time of the MockClock.
  void AdvanceTime(QuicTime::Delta delta);

  MOCK_METHOD3(ProcessUdpPacket, void(const IPEndPoint& self_address,
                                      const IPEndPoint& peer_address,
                                      const QuicEncryptedPacket& packet));
  MOCK_METHOD1(SendConnectionClose, void(QuicErrorCode error));
  MOCK_METHOD2(SendConnectionCloseWithDetails,
               void(QuicErrorCode error, const std::string& details));
  MOCK_METHOD2(SendConnectionClosePacket,
               void(QuicErrorCode error, const std::string& details));
  MOCK_METHOD3(SendRstStream, void(QuicStreamId id,
                                   QuicRstStreamErrorCode error,
                                   QuicStreamOffset bytes_written));
  MOCK_METHOD3(SendGoAway,
               void(QuicErrorCode error,
                    QuicStreamId last_good_stream_id,
                    const std::string& reason));
  MOCK_METHOD1(SendBlocked, void(QuicStreamId id));
  MOCK_METHOD2(SendWindowUpdate, void(QuicStreamId id,
                                      QuicStreamOffset byte_offset));
  MOCK_METHOD0(OnCanWrite, void());

  MOCK_METHOD1(ResumeConnectionState, bool(const CachedNetworkParameters&));

  void ReallyProcessUdpPacket(const IPEndPoint& self_address,
                              const IPEndPoint& peer_address,
                              const QuicEncryptedPacket& packet) {
    QuicConnection::ProcessUdpPacket(self_address, peer_address, packet);
  }

  bool OnProtocolVersionMismatch(QuicVersion version) override {
    return false;
  }

 private:
  scoped_ptr<QuicConnectionHelperInterface> helper_;

  DISALLOW_COPY_AND_ASSIGN(MockConnection);
};

class PacketSavingConnection : public MockConnection {
 public:
  explicit PacketSavingConnection(bool is_server);

  PacketSavingConnection(bool is_server,
                         const QuicVersionVector& supported_versions);

  ~PacketSavingConnection() override;

  void SendOrQueuePacket(QueuedPacket packet) override;

  std::vector<QuicEncryptedPacket*> encrypted_packets_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PacketSavingConnection);
};

class MockSession : public QuicSession {
 public:
  explicit MockSession(QuicConnection* connection);
  ~MockSession() override;
  MOCK_METHOD2(OnConnectionClosed, void(QuicErrorCode error, bool from_peer));
  MOCK_METHOD1(CreateIncomingDataStream, QuicDataStream*(QuicStreamId id));
  MOCK_METHOD0(GetCryptoStream, QuicCryptoStream*());
  MOCK_METHOD0(CreateOutgoingDataStream, QuicDataStream*());
  MOCK_METHOD6(WritevData,
               QuicConsumedData(QuicStreamId id,
                                const IOVector& data,
                                QuicStreamOffset offset,
                                bool fin,
                                FecProtection fec_protection,
                                QuicAckNotifier::DelegateInterface*));
  MOCK_METHOD2(OnStreamHeaders, void(QuicStreamId stream_id,
                                     base::StringPiece headers_data));
  MOCK_METHOD2(OnStreamHeadersPriority, void(QuicStreamId stream_id,
                                             QuicPriority priority));
  MOCK_METHOD3(OnStreamHeadersComplete, void(QuicStreamId stream_id,
                                             bool fin,
                                             size_t frame_len));
  MOCK_METHOD3(SendRstStream, void(QuicStreamId stream_id,
                                   QuicRstStreamErrorCode error,
                                   QuicStreamOffset bytes_written));
  MOCK_METHOD0(IsCryptoHandshakeConfirmed, bool());

  using QuicSession::ActivateStream;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSession);
};

class TestSession : public QuicSession {
 public:
  TestSession(QuicConnection* connection, const QuicConfig& config);
  ~TestSession() override;

  MOCK_METHOD1(CreateIncomingDataStream, QuicDataStream*(QuicStreamId id));
  MOCK_METHOD0(CreateOutgoingDataStream, QuicDataStream*());

  void SetCryptoStream(QuicCryptoStream* stream);

  QuicCryptoStream* GetCryptoStream() override;

 private:
  QuicCryptoStream* crypto_stream_;

  DISALLOW_COPY_AND_ASSIGN(TestSession);
};

class TestClientSession : public QuicClientSessionBase {
 public:
  TestClientSession(QuicConnection* connection, const QuicConfig& config);
  ~TestClientSession() override;

  // QuicClientSessionBase
  MOCK_METHOD1(OnProofValid,
               void(const QuicCryptoClientConfig::CachedState& cached));
  MOCK_METHOD1(OnProofVerifyDetailsAvailable,
               void(const ProofVerifyDetails& verify_details));

  // TestClientSession
  MOCK_METHOD1(CreateIncomingDataStream, QuicDataStream*(QuicStreamId id));
  MOCK_METHOD0(CreateOutgoingDataStream, QuicDataStream*());

  void SetCryptoStream(QuicCryptoStream* stream);

  QuicCryptoStream* GetCryptoStream() override;

 private:
  QuicCryptoStream* crypto_stream_;

  DISALLOW_COPY_AND_ASSIGN(TestClientSession);
};

class MockPacketWriter : public QuicPacketWriter {
 public:
  MockPacketWriter();
  ~MockPacketWriter() override;

  MOCK_METHOD4(WritePacket,
               WriteResult(const char* buffer,
                           size_t buf_len,
                           const IPAddressNumber& self_address,
                           const IPEndPoint& peer_address));
  MOCK_CONST_METHOD0(IsWriteBlockedDataBuffered, bool());
  MOCK_CONST_METHOD0(IsWriteBlocked, bool());
  MOCK_METHOD0(SetWritable, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPacketWriter);
};

class MockSendAlgorithm : public SendAlgorithmInterface {
 public:
  MockSendAlgorithm();
  ~MockSendAlgorithm() override;

  MOCK_METHOD3(SetFromConfig, void(const QuicConfig& config,
                                   bool is_server,
                                   bool using_pacing));
  MOCK_METHOD1(SetNumEmulatedConnections, void(int num_connections));
  MOCK_METHOD1(SetMaxPacketSize, void(QuicByteCount max_packet_size));
  MOCK_METHOD4(OnCongestionEvent, void(bool rtt_updated,
                                       QuicByteCount bytes_in_flight,
                                       const CongestionVector& acked_packets,
                                       const CongestionVector& lost_packets));
  MOCK_METHOD5(OnPacketSent,
               bool(QuicTime, QuicByteCount, QuicPacketSequenceNumber,
                    QuicByteCount, HasRetransmittableData));
  MOCK_METHOD1(OnRetransmissionTimeout, void(bool));
  MOCK_METHOD0(RevertRetransmissionTimeout, void());
  MOCK_CONST_METHOD3(TimeUntilSend,
                     QuicTime::Delta(QuicTime now,
                                     QuicByteCount bytes_in_flight,
                                     HasRetransmittableData));
  MOCK_CONST_METHOD0(PacingRate, QuicBandwidth(void));
  MOCK_CONST_METHOD0(BandwidthEstimate, QuicBandwidth(void));
  MOCK_CONST_METHOD0(HasReliableBandwidthEstimate, bool());
  MOCK_METHOD1(OnRttUpdated, void(QuicPacketSequenceNumber));
  MOCK_CONST_METHOD0(RetransmissionDelay, QuicTime::Delta(void));
  MOCK_CONST_METHOD0(GetCongestionWindow, QuicByteCount());
  MOCK_CONST_METHOD0(InSlowStart, bool());
  MOCK_CONST_METHOD0(InRecovery, bool());
  MOCK_CONST_METHOD0(GetSlowStartThreshold, QuicByteCount());
  MOCK_CONST_METHOD0(GetCongestionControlType, CongestionControlType());
  MOCK_METHOD1(ResumeConnectionState, bool(const CachedNetworkParameters&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSendAlgorithm);
};

class MockLossAlgorithm : public LossDetectionInterface {
 public:
  MockLossAlgorithm();
  ~MockLossAlgorithm() override;

  MOCK_CONST_METHOD0(GetLossDetectionType, LossDetectionType());
  MOCK_METHOD4(DetectLostPackets,
               SequenceNumberSet(const QuicUnackedPacketMap& unacked_packets,
                                 const QuicTime& time,
                                 QuicPacketSequenceNumber largest_observed,
                                 const RttStats& rtt_stats));
  MOCK_CONST_METHOD0(GetLossTimeout, QuicTime());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockLossAlgorithm);
};

class TestEntropyCalculator :
      public QuicReceivedEntropyHashCalculatorInterface {
 public:
  TestEntropyCalculator();
  ~TestEntropyCalculator() override;

  QuicPacketEntropyHash EntropyHash(
      QuicPacketSequenceNumber sequence_number) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestEntropyCalculator);
};

class MockEntropyCalculator : public TestEntropyCalculator {
 public:
  MockEntropyCalculator();
  ~MockEntropyCalculator() override;

  MOCK_CONST_METHOD1(
      EntropyHash,
      QuicPacketEntropyHash(QuicPacketSequenceNumber sequence_number));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEntropyCalculator);
};

class MockAckNotifierDelegate : public QuicAckNotifier::DelegateInterface {
 public:
  MockAckNotifierDelegate();

  MOCK_METHOD3(OnAckNotification,
               void(int num_retransmitted_packets,
                    int num_retransmitted_bytes,
                    QuicTime::Delta delta_largest_observed));

 protected:
  // Object is ref counted.
  ~MockAckNotifierDelegate() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAckNotifierDelegate);
};

class MockNetworkChangeVisitor :
      public QuicSentPacketManager::NetworkChangeVisitor {
 public:
  MockNetworkChangeVisitor();
  ~MockNetworkChangeVisitor() override;

  MOCK_METHOD0(OnCongestionWindowChange, void());
  MOCK_METHOD0(OnRttChange, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNetworkChangeVisitor);
};

// Creates per-connection packet writers that register themselves with the
// TestWriterFactory on each write so that TestWriterFactory::OnPacketSent can
// be routed to the appropriate QuicConnection.
class TestWriterFactory : public QuicDispatcher::PacketWriterFactory {
 public:
  TestWriterFactory();
  ~TestWriterFactory() override;

  QuicPacketWriter* Create(QuicServerPacketWriter* writer,
                           QuicConnection* connection) override;

  // Calls OnPacketSent on the last QuicConnection to write through one of the
  // packet writers created by this factory.
  void OnPacketSent(WriteResult result);

 private:
  class PerConnectionPacketWriter : public QuicPerConnectionPacketWriter {
   public:
    PerConnectionPacketWriter(TestWriterFactory* factory,
                              QuicServerPacketWriter* writer,
                              QuicConnection* connection);
    ~PerConnectionPacketWriter() override;

    WriteResult WritePacket(const char* buffer,
                            size_t buf_len,
                            const IPAddressNumber& self_address,
                            const IPEndPoint& peer_address) override;

   private:
    TestWriterFactory* factory_;
  };

  // If an asynchronous write is happening and |writer| gets deleted, this
  // clears the pointer to it to prevent use-after-free.
  void Unregister(PerConnectionPacketWriter* writer);

  PerConnectionPacketWriter* current_writer_;
};

}  // namespace test
}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_
