// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/port.h"
#include "base/stl_util.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_framer_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"

using base::hash_set;
using base::StringPiece;
using std::make_pair;
using std::map;
using std::numeric_limits;
using std::string;
using std::vector;
using testing::Return;
using testing::_;

namespace net {
namespace test {

const QuicPacketSequenceNumber kEpoch = GG_UINT64_C(1) << 48;
const QuicPacketSequenceNumber kMask = kEpoch - 1;

// Index into the flags offset in the header.
const size_t kPublicFlagsOffset = 0;
// Index into the guid offset in the header.
const size_t kGuidOffset = kPublicFlagsSize;
// Index into the version string in the header. (if present).
const size_t kVersionOffset = kGuidOffset + PACKET_8BYTE_GUID;

// Index into the sequence number offset in the header.
size_t GetSequenceNumberOffset(QuicGuidLength guid_length,
                               bool include_version) {
  return kGuidOffset + guid_length +
      (include_version ? kQuicVersionSize : 0);
}

size_t GetSequenceNumberOffset(bool include_version) {
  return GetSequenceNumberOffset(PACKET_8BYTE_GUID, include_version);
}

// Index into the private flags offset in the data packet header.
size_t GetPrivateFlagsOffset(QuicGuidLength guid_length, bool include_version) {
  return GetSequenceNumberOffset(guid_length, include_version) +
      PACKET_6BYTE_SEQUENCE_NUMBER;
}

size_t GetPrivateFlagsOffset(bool include_version) {
  return GetPrivateFlagsOffset(PACKET_8BYTE_GUID, include_version);
}

size_t GetPrivateFlagsOffset(bool include_version,
                             QuicSequenceNumberLength sequence_number_length) {
  return GetSequenceNumberOffset(PACKET_8BYTE_GUID, include_version) +
      sequence_number_length;
}

// Index into the fec group offset in the header.
size_t GetFecGroupOffset(QuicGuidLength guid_length, bool include_version) {
  return GetPrivateFlagsOffset(guid_length, include_version) +
      kPrivateFlagsSize;
}

size_t GetFecGroupOffset(bool include_version) {
  return GetPrivateFlagsOffset(PACKET_8BYTE_GUID, include_version) +
      kPrivateFlagsSize;
}

size_t GetFecGroupOffset(bool include_version,
                         QuicSequenceNumberLength sequence_number_length) {
  return GetPrivateFlagsOffset(include_version, sequence_number_length) +
      kPrivateFlagsSize;
}

// Index into the nonce proof of the public reset packet.
// Public resets always have full guids.
const size_t kPublicResetPacketNonceProofOffset =
    kGuidOffset + PACKET_8BYTE_GUID;

// Index into the rejected sequence number of the public reset packet.
const size_t kPublicResetPacketRejectedSequenceNumberOffset =
    kPublicResetPacketNonceProofOffset + kPublicResetNonceSize;

class TestEncrypter : public QuicEncrypter {
 public:
  virtual ~TestEncrypter() {}
  virtual bool SetKey(StringPiece key) OVERRIDE {
    return true;
  }
  virtual bool SetNoncePrefix(StringPiece nonce_prefix) OVERRIDE {
    return true;
  }
  virtual bool Encrypt(StringPiece nonce,
                       StringPiece associated_data,
                       StringPiece plaintext,
                       unsigned char* output) OVERRIDE {
    CHECK(false) << "Not implemented";
    return false;
  }
  virtual QuicData* EncryptPacket(QuicPacketSequenceNumber sequence_number,
                                  StringPiece associated_data,
                                  StringPiece plaintext) OVERRIDE {
    sequence_number_ = sequence_number;
    associated_data_ = associated_data.as_string();
    plaintext_ = plaintext.as_string();
    return new QuicData(plaintext.data(), plaintext.length());
  }
  virtual size_t GetKeySize() const OVERRIDE {
    return 0;
  }
  virtual size_t GetNoncePrefixSize() const OVERRIDE {
    return 0;
  }
  virtual size_t GetMaxPlaintextSize(size_t ciphertext_size) const OVERRIDE {
    return ciphertext_size;
  }
  virtual size_t GetCiphertextSize(size_t plaintext_size) const OVERRIDE {
    return plaintext_size;
  }
  virtual StringPiece GetKey() const OVERRIDE {
    return StringPiece();
  }
  virtual StringPiece GetNoncePrefix() const OVERRIDE {
    return StringPiece();
  }
  QuicPacketSequenceNumber sequence_number_;
  string associated_data_;
  string plaintext_;
};

class TestDecrypter : public QuicDecrypter {
 public:
  virtual ~TestDecrypter() {}
  virtual bool SetKey(StringPiece key) OVERRIDE {
    return true;
  }
  virtual bool SetNoncePrefix(StringPiece nonce_prefix) OVERRIDE {
    return true;
  }
  virtual bool Decrypt(StringPiece nonce,
                       StringPiece associated_data,
                       StringPiece ciphertext,
                       unsigned char* output,
                       size_t* output_length) OVERRIDE {
    CHECK(false) << "Not implemented";
    return false;
  }
  virtual QuicData* DecryptPacket(QuicPacketSequenceNumber sequence_number,
                                  StringPiece associated_data,
                                  StringPiece ciphertext) OVERRIDE {
    sequence_number_ = sequence_number;
    associated_data_ = associated_data.as_string();
    ciphertext_ = ciphertext.as_string();
    return new QuicData(ciphertext.data(), ciphertext.length());
  }
  virtual StringPiece GetKey() const OVERRIDE {
    return StringPiece();
  }
  virtual StringPiece GetNoncePrefix() const OVERRIDE {
    return StringPiece();
  }
  QuicPacketSequenceNumber sequence_number_;
  string associated_data_;
  string ciphertext_;
};

class TestQuicVisitor : public ::net::QuicFramerVisitorInterface {
 public:
  TestQuicVisitor()
      : error_count_(0),
        version_mismatch_(0),
        packet_count_(0),
        frame_count_(0),
        fec_count_(0),
        complete_packets_(0),
        revived_packets_(0),
        accept_packet_(true) {
  }

  virtual ~TestQuicVisitor() {
    STLDeleteElements(&stream_frames_);
    STLDeleteElements(&ack_frames_);
    STLDeleteElements(&congestion_feedback_frames_);
    STLDeleteElements(&fec_data_);
  }

  virtual void OnError(QuicFramer* f) OVERRIDE {
    DLOG(INFO) << "QuicFramer Error: " << QuicUtils::ErrorToString(f->error())
               << " (" << f->error() << ")";
    error_count_++;
  }

  virtual void OnPacket() OVERRIDE {}

  virtual void OnPublicResetPacket(
      const QuicPublicResetPacket& packet) OVERRIDE {
    public_reset_packet_.reset(new QuicPublicResetPacket(packet));
  }

  virtual void OnVersionNegotiationPacket(
      const QuicVersionNegotiationPacket& packet) OVERRIDE {
    version_negotiation_packet_.reset(new QuicVersionNegotiationPacket(packet));
  }

  virtual void OnRevivedPacket() OVERRIDE {
    revived_packets_++;
  }

  virtual bool OnProtocolVersionMismatch(QuicVersion version) OVERRIDE {
    DLOG(INFO) << "QuicFramer Version Mismatch, version: " << version;
    version_mismatch_++;
    return true;
  }

  virtual bool OnPacketHeader(const QuicPacketHeader& header) OVERRIDE {
    packet_count_++;
    header_.reset(new QuicPacketHeader(header));
    return accept_packet_;
  }

  virtual bool OnStreamFrame(const QuicStreamFrame& frame) OVERRIDE {
    frame_count_++;
    stream_frames_.push_back(new QuicStreamFrame(frame));
    return true;
  }

  virtual void OnFecProtectedPayload(StringPiece payload) OVERRIDE {
    fec_protected_payload_ = payload.as_string();
  }

  virtual bool OnAckFrame(const QuicAckFrame& frame) OVERRIDE {
    frame_count_++;
    ack_frames_.push_back(new QuicAckFrame(frame));
    return true;
  }

  virtual bool OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) OVERRIDE {
    frame_count_++;
    congestion_feedback_frames_.push_back(
        new QuicCongestionFeedbackFrame(frame));
    return true;
  }

  virtual void OnFecData(const QuicFecData& fec) OVERRIDE {
    fec_count_++;
    fec_data_.push_back(new QuicFecData(fec));
  }

  virtual void OnPacketComplete() OVERRIDE {
    complete_packets_++;
  }

  virtual bool OnRstStreamFrame(const QuicRstStreamFrame& frame) OVERRIDE {
    rst_stream_frame_ = frame;
    return true;
  }

  virtual bool OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) OVERRIDE {
    connection_close_frame_ = frame;
    return true;
  }

  virtual bool OnGoAwayFrame(const QuicGoAwayFrame& frame) OVERRIDE {
    goaway_frame_ = frame;
    return true;
  }

  // Counters from the visitor_ callbacks.
  int error_count_;
  int version_mismatch_;
  int packet_count_;
  int frame_count_;
  int fec_count_;
  int complete_packets_;
  int revived_packets_;
  bool accept_packet_;

  scoped_ptr<QuicPacketHeader> header_;
  scoped_ptr<QuicPublicResetPacket> public_reset_packet_;
  scoped_ptr<QuicVersionNegotiationPacket> version_negotiation_packet_;
  vector<QuicStreamFrame*> stream_frames_;
  vector<QuicAckFrame*> ack_frames_;
  vector<QuicCongestionFeedbackFrame*> congestion_feedback_frames_;
  vector<QuicFecData*> fec_data_;
  string fec_protected_payload_;
  QuicRstStreamFrame rst_stream_frame_;
  QuicConnectionCloseFrame connection_close_frame_;
  QuicGoAwayFrame goaway_frame_;
};

class QuicFramerTest : public ::testing::TestWithParam<QuicVersion> {
 public:
  QuicFramerTest()
      : encrypter_(new test::TestEncrypter()),
        decrypter_(new test::TestDecrypter()),
        start_(QuicTime::Zero().Add(QuicTime::Delta::FromMicroseconds(0x10))),
        framer_(QuicVersionMax(), start_, true) {
    framer_.SetDecrypter(decrypter_);
    framer_.SetEncrypter(ENCRYPTION_NONE, encrypter_);
    framer_.set_visitor(&visitor_);
    framer_.set_received_entropy_calculator(&entropy_calculator_);

    version_ = GetParam();
    framer_.set_version(version_);
  }

  bool CheckEncryption(QuicPacketSequenceNumber sequence_number,
                       QuicPacket* packet) {
    if (sequence_number != encrypter_->sequence_number_) {
      LOG(ERROR) << "Encrypted incorrect packet sequence number.  expected "
                 << sequence_number << " actual: "
                 << encrypter_->sequence_number_;
      return false;
    }
    if (packet->AssociatedData() != encrypter_->associated_data_) {
      LOG(ERROR) << "Encrypted incorrect associated data.  expected "
                 << packet->AssociatedData() << " actual: "
                 << encrypter_->associated_data_;
      return false;
    }
    if (packet->Plaintext() != encrypter_->plaintext_) {
      LOG(ERROR) << "Encrypted incorrect plaintext data.  expected "
                 << packet->Plaintext() << " actual: "
                 << encrypter_->plaintext_;
      return false;
    }
    return true;
  }

  bool CheckDecryption(const QuicEncryptedPacket& encrypted,
                       bool includes_version) {
    if (visitor_.header_->packet_sequence_number !=
        decrypter_->sequence_number_) {
      LOG(ERROR) << "Decrypted incorrect packet sequence number.  expected "
                 << visitor_.header_->packet_sequence_number << " actual: "
                 << decrypter_->sequence_number_;
      return false;
    }
    if (QuicFramer::GetAssociatedDataFromEncryptedPacket(
            encrypted, PACKET_8BYTE_GUID,
            includes_version, PACKET_6BYTE_SEQUENCE_NUMBER) !=
        decrypter_->associated_data_) {
      LOG(ERROR) << "Decrypted incorrect associated data.  expected "
                 << QuicFramer::GetAssociatedDataFromEncryptedPacket(
                     encrypted, PACKET_8BYTE_GUID,
                     includes_version, PACKET_6BYTE_SEQUENCE_NUMBER)
                 << " actual: " << decrypter_->associated_data_;
      return false;
    }
    StringPiece ciphertext(encrypted.AsStringPiece().substr(
        GetStartOfEncryptedData(PACKET_8BYTE_GUID, includes_version,
                                PACKET_6BYTE_SEQUENCE_NUMBER)));
    if (ciphertext != decrypter_->ciphertext_) {
      LOG(ERROR) << "Decrypted incorrect ciphertext data.  expected "
                 << ciphertext << " actual: "
                 << decrypter_->ciphertext_;
      return false;
    }
    return true;
  }

  char* AsChars(unsigned char* data) {
    return reinterpret_cast<char*>(data);
  }

  void CheckProcessingFails(unsigned char* packet,
                            size_t len,
                            string expected_error,
                            QuicErrorCode error_code) {
    QuicEncryptedPacket encrypted(AsChars(packet), len, false);
    EXPECT_FALSE(framer_.ProcessPacket(encrypted)) << "len: " << len;
    EXPECT_EQ(expected_error, framer_.detailed_error()) << "len: " << len;
    EXPECT_EQ(error_code, framer_.error()) << "len: " << len;
  }

  void ValidateTruncatedAck(const QuicAckFrame* ack, size_t keys) {
    for (size_t i = 1; i < keys; ++i) {
      EXPECT_TRUE(ContainsKey(ack->received_info.missing_packets, i)) << i;
    }
    EXPECT_EQ(keys, ack->received_info.largest_observed);
  }

  void CheckCalculatePacketSequenceNumber(
      QuicPacketSequenceNumber expected_sequence_number,
      QuicPacketSequenceNumber last_sequence_number) {
    QuicPacketSequenceNumber wire_sequence_number =
        expected_sequence_number & kMask;
    QuicFramerPeer::SetLastSequenceNumber(&framer_, last_sequence_number);
    EXPECT_EQ(expected_sequence_number,
              QuicFramerPeer::CalculatePacketSequenceNumberFromWire(
                  &framer_, PACKET_6BYTE_SEQUENCE_NUMBER, wire_sequence_number))
        << "last_sequence_number: " << last_sequence_number
        << " wire_sequence_number: " << wire_sequence_number;
  }

  test::TestEncrypter* encrypter_;
  test::TestDecrypter* decrypter_;
  QuicVersion version_;
  QuicTime start_;
  QuicFramer framer_;
  test::TestQuicVisitor visitor_;
  test::TestEntropyCalculator entropy_calculator_;
};

// Run all framer tests with all supported versions of QUIC.
INSTANTIATE_TEST_CASE_P(QuicFramerTests,
                        QuicFramerTest,
                        ::testing::ValuesIn(kSupportedQuicVersions));

TEST_P(QuicFramerTest, CalculatePacketSequenceNumberFromWireNearEpochStart) {
  // A few quick manual sanity checks
  CheckCalculatePacketSequenceNumber(GG_UINT64_C(1), GG_UINT64_C(0));
  CheckCalculatePacketSequenceNumber(kEpoch + 1, kMask);
  CheckCalculatePacketSequenceNumber(kEpoch, kMask);

  // Cases where the last number was close to the start of the range
  for (uint64 last = 0; last < 10; last++) {
    // Small numbers should not wrap (even if they're out of order).
    for (uint64 j = 0; j < 10; j++) {
      CheckCalculatePacketSequenceNumber(j, last);
    }

    // Large numbers should not wrap either (because we're near 0 already).
    for (uint64 j = 0; j < 10; j++) {
      CheckCalculatePacketSequenceNumber(kEpoch - 1 - j, last);
    }
  }
}

TEST_P(QuicFramerTest, CalculatePacketSequenceNumberFromWireNearEpochEnd) {
  // Cases where the last number was close to the end of the range
  for (uint64 i = 0; i < 10; i++) {
    QuicPacketSequenceNumber last = kEpoch - i;

    // Small numbers should wrap.
    for (uint64 j = 0; j < 10; j++) {
      CheckCalculatePacketSequenceNumber(kEpoch + j, last);
    }

    // Large numbers should not (even if they're out of order).
    for (uint64 j = 0; j < 10; j++) {
      CheckCalculatePacketSequenceNumber(kEpoch - 1 - j, last);
    }
  }
}

// Next check where we're in a non-zero epoch to verify we handle
// reverse wrapping, too.
TEST_P(QuicFramerTest, CalculatePacketSequenceNumberFromWireNearPrevEpoch) {
  const uint64 prev_epoch = 1 * kEpoch;
  const uint64 cur_epoch = 2 * kEpoch;
  // Cases where the last number was close to the start of the range
  for (uint64 i = 0; i < 10; i++) {
    uint64 last = cur_epoch + i;
    // Small number should not wrap (even if they're out of order).
    for (uint64 j = 0; j < 10; j++) {
      CheckCalculatePacketSequenceNumber(cur_epoch + j, last);
    }

    // But large numbers should reverse wrap.
    for (uint64 j = 0; j < 10; j++) {
      uint64 num = kEpoch - 1 - j;
      CheckCalculatePacketSequenceNumber(prev_epoch + num, last);
    }
  }
}

TEST_P(QuicFramerTest, CalculatePacketSequenceNumberFromWireNearNextEpoch) {
  const uint64 cur_epoch = 2 * kEpoch;
  const uint64 next_epoch = 3 * kEpoch;
  // Cases where the last number was close to the end of the range
  for (uint64 i = 0; i < 10; i++) {
    QuicPacketSequenceNumber last = next_epoch - 1 - i;

    // Small numbers should wrap.
    for (uint64 j = 0; j < 10; j++) {
      CheckCalculatePacketSequenceNumber(next_epoch + j, last);
    }

    // but large numbers should not (even if they're out of order).
    for (uint64 j = 0; j < 10; j++) {
      uint64 num = kEpoch - 1 - j;
      CheckCalculatePacketSequenceNumber(cur_epoch + num, last);
    }
  }
}

TEST_P(QuicFramerTest, CalculatePacketSequenceNumberFromWireNearNextMax) {
  const uint64 max_number = numeric_limits<uint64>::max();
  const uint64 max_epoch = max_number & ~kMask;

  // Cases where the last number was close to the end of the range
  for (uint64 i = 0; i < 10; i++) {
    // Subtract 1, because the expected next sequence number is 1 more than the
    // last sequence number.
    QuicPacketSequenceNumber last = max_number - i - 1;

    // Small numbers should not wrap, because they have nowhere to go.
    for (uint64 j = 0; j < 10; j++) {
      CheckCalculatePacketSequenceNumber(max_epoch + j, last);
    }

    // Large numbers should not wrap either.
    for (uint64 j = 0; j < 10; j++) {
      uint64 num = kEpoch - 1 - j;
      CheckCalculatePacketSequenceNumber(max_epoch + num, last);
    }
  }
}

TEST_P(QuicFramerTest, EmptyPacket) {
  char packet[] = { 0x00 };
  QuicEncryptedPacket encrypted(packet, 0, false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_INVALID_PACKET_HEADER, framer_.error());
}

TEST_P(QuicFramerTest, LargePacket) {
  unsigned char packet[kMaxPacketSize + 1] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,
  };

  memset(packet + GetPacketHeaderSize(
             PACKET_8BYTE_GUID, !kIncludeVersion,
             PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP), 0,
         kMaxPacketSize - GetPacketHeaderSize(
             PACKET_8BYTE_GUID, !kIncludeVersion,
             PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP) + 1);

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));

  ASSERT_TRUE(visitor_.header_.get());
  // Make sure we've parsed the packet header, so we can send an error.
  EXPECT_EQ(GG_UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.guid);
  // Make sure the correct error is propagated.
  EXPECT_EQ(QUIC_PACKET_TOO_LARGE, framer_.error());
}

TEST_P(QuicFramerTest, PacketHeader) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(GG_UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.guid);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.header_->public_header.version_flag);
  EXPECT_FALSE(visitor_.header_->fec_flag);
  EXPECT_FALSE(visitor_.header_->entropy_flag);
  EXPECT_EQ(0, visitor_.header_->entropy_hash);
  EXPECT_EQ(GG_UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  // Now test framing boundaries
  for (size_t i = 0;
       i < GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                               PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
       ++i) {
    string expected_error;
    if (i < kGuidOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < GetSequenceNumberOffset(!kIncludeVersion)) {
      expected_error = "Unable to read GUID.";
    } else if (i < GetPrivateFlagsOffset(!kIncludeVersion)) {
      expected_error = "Unable to read sequence number.";
    } else if (i < GetFecGroupOffset(!kIncludeVersion)) {
      expected_error = "Unable to read private flags.";
    } else {
      expected_error = "Unable to read first fec protected packet offset.";
    }
    CheckProcessingFails(packet, i, expected_error, QUIC_INVALID_PACKET_HEADER);
  }
}

TEST_P(QuicFramerTest, PacketHeaderWith4ByteGuid) {
  QuicFramerPeer::SetLastSerializedGuid(&framer_,
                                        GG_UINT64_C(0xFEDCBA9876543210));

  unsigned char packet[] = {
    // public flags (4 byte guid)
    0x38,
    // guid
    0x10, 0x32, 0x54, 0x76,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(GG_UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.guid);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.header_->public_header.version_flag);
  EXPECT_FALSE(visitor_.header_->fec_flag);
  EXPECT_FALSE(visitor_.header_->entropy_flag);
  EXPECT_EQ(0, visitor_.header_->entropy_hash);
  EXPECT_EQ(GG_UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  // Now test framing boundaries
  for (size_t i = 0;
       i < GetPacketHeaderSize(PACKET_4BYTE_GUID, !kIncludeVersion,
                               PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
       ++i) {
    string expected_error;
    if (i < kGuidOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < GetSequenceNumberOffset(PACKET_4BYTE_GUID,
                                           !kIncludeVersion)) {
      expected_error = "Unable to read GUID.";
    } else if (i < GetPrivateFlagsOffset(PACKET_4BYTE_GUID,
                                         !kIncludeVersion)) {
      expected_error = "Unable to read sequence number.";
    } else if (i < GetFecGroupOffset(PACKET_4BYTE_GUID, !kIncludeVersion)) {
      expected_error = "Unable to read private flags.";
    } else {
      expected_error = "Unable to read first fec protected packet offset.";
    }
    CheckProcessingFails(packet, i, expected_error, QUIC_INVALID_PACKET_HEADER);
  }
}

TEST_P(QuicFramerTest, PacketHeader1ByteGuid) {
  QuicFramerPeer::SetLastSerializedGuid(&framer_,
                                        GG_UINT64_C(0xFEDCBA9876543210));

  unsigned char packet[] = {
    // public flags (1 byte guid)
    0x34,
    // guid
    0x10,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(GG_UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.guid);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.header_->public_header.version_flag);
  EXPECT_FALSE(visitor_.header_->fec_flag);
  EXPECT_FALSE(visitor_.header_->entropy_flag);
  EXPECT_EQ(0, visitor_.header_->entropy_hash);
  EXPECT_EQ(GG_UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  // Now test framing boundaries
  for (size_t i = 0;
       i < GetPacketHeaderSize(PACKET_1BYTE_GUID, !kIncludeVersion,
                               PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
       ++i) {
    string expected_error;
    if (i < kGuidOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < GetSequenceNumberOffset(PACKET_1BYTE_GUID,
                                           !kIncludeVersion)) {
      expected_error = "Unable to read GUID.";
    } else if (i < GetPrivateFlagsOffset(PACKET_1BYTE_GUID, !kIncludeVersion)) {
      expected_error = "Unable to read sequence number.";
    } else if (i < GetFecGroupOffset(PACKET_1BYTE_GUID, !kIncludeVersion)) {
      expected_error = "Unable to read private flags.";
    } else {
      expected_error = "Unable to read first fec protected packet offset.";
    }
    CheckProcessingFails(packet, i, expected_error, QUIC_INVALID_PACKET_HEADER);
  }
}

TEST_P(QuicFramerTest, PacketHeaderWith0ByteGuid) {
  QuicFramerPeer::SetLastSerializedGuid(&framer_,
                                        GG_UINT64_C(0xFEDCBA9876543210));

  unsigned char packet[] = {
    // public flags (0 byte guid)
    0x30,
    // guid
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(GG_UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.guid);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.header_->public_header.version_flag);
  EXPECT_FALSE(visitor_.header_->fec_flag);
  EXPECT_FALSE(visitor_.header_->entropy_flag);
  EXPECT_EQ(0, visitor_.header_->entropy_hash);
  EXPECT_EQ(GG_UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  // Now test framing boundaries
  for (size_t i = 0;
       i < GetPacketHeaderSize(PACKET_0BYTE_GUID, !kIncludeVersion,
                               PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
       ++i) {
    string expected_error;
    if (i < kGuidOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < GetSequenceNumberOffset(PACKET_0BYTE_GUID,
                                           !kIncludeVersion)) {
      expected_error = "Unable to read GUID.";
    } else if (i < GetPrivateFlagsOffset(PACKET_0BYTE_GUID, !kIncludeVersion)) {
      expected_error = "Unable to read sequence number.";
    } else if (i < GetFecGroupOffset(PACKET_0BYTE_GUID, !kIncludeVersion)) {
      expected_error = "Unable to read private flags.";
    } else {
      expected_error = "Unable to read first fec protected packet offset.";
    }
    CheckProcessingFails(packet, i, expected_error, QUIC_INVALID_PACKET_HEADER);
  }
}

TEST_P(QuicFramerTest, PacketHeaderWithVersionFlag) {
  // Set a specific version.
  framer_.set_version(QUIC_VERSION_7);

  unsigned char packet[] = {
    // public flags (version)
    0x3D,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // version tag
    'Q', '0', '0', '7',
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(GG_UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.guid);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_TRUE(visitor_.header_->public_header.version_flag);
  EXPECT_EQ(QUIC_VERSION_7, visitor_.header_->public_header.versions[0]);
  EXPECT_FALSE(visitor_.header_->fec_flag);
  EXPECT_FALSE(visitor_.header_->entropy_flag);
  EXPECT_EQ(0, visitor_.header_->entropy_hash);
  EXPECT_EQ(GG_UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  // Now test framing boundaries
  for (size_t i = 0;
       i < GetPacketHeaderSize(PACKET_8BYTE_GUID, kIncludeVersion,
                               PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
       ++i) {
    string expected_error;
    if (i < kGuidOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < kVersionOffset) {
      expected_error = "Unable to read GUID.";
    } else if (i <  GetSequenceNumberOffset(kIncludeVersion)) {
      expected_error = "Unable to read protocol version.";
    } else if (i < GetPrivateFlagsOffset(kIncludeVersion)) {
      expected_error = "Unable to read sequence number.";
    } else if (i < GetFecGroupOffset(kIncludeVersion)) {
      expected_error = "Unable to read private flags.";
    } else {
      expected_error = "Unable to read first fec protected packet offset.";
    }
    CheckProcessingFails(packet, i, expected_error, QUIC_INVALID_PACKET_HEADER);
  }
}

TEST_P(QuicFramerTest, PacketHeaderWith4ByteSequenceNumber) {
  QuicFramerPeer::SetLastSequenceNumber(&framer_,
                                        GG_UINT64_C(0x123456789ABA));

  unsigned char packet[] = {
    // public flags (8 byte guid and 4 byte sequence number)
    0x2C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    // private flags
    0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(GG_UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.guid);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.header_->public_header.version_flag);
  EXPECT_FALSE(visitor_.header_->fec_flag);
  EXPECT_FALSE(visitor_.header_->entropy_flag);
  EXPECT_EQ(0, visitor_.header_->entropy_hash);
  EXPECT_EQ(GG_UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  // Now test framing boundaries
  for (size_t i = 0;
       i < GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                               PACKET_4BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
       ++i) {
    string expected_error;
    if (i < kGuidOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < GetSequenceNumberOffset(!kIncludeVersion)) {
      expected_error = "Unable to read GUID.";
    } else if (i < GetPrivateFlagsOffset(!kIncludeVersion,
                                         PACKET_4BYTE_SEQUENCE_NUMBER)) {
      expected_error = "Unable to read sequence number.";
    } else if (i < GetFecGroupOffset(!kIncludeVersion,
                                     PACKET_4BYTE_SEQUENCE_NUMBER)) {
      expected_error = "Unable to read private flags.";
    } else {
      expected_error = "Unable to read first fec protected packet offset.";
    }
    CheckProcessingFails(packet, i, expected_error, QUIC_INVALID_PACKET_HEADER);
  }
}

TEST_P(QuicFramerTest, PacketHeaderWith2ByteSequenceNumber) {
  QuicFramerPeer::SetLastSequenceNumber(&framer_,
                                        GG_UINT64_C(0x123456789ABA));

  unsigned char packet[] = {
    // public flags (8 byte guid and 2 byte sequence number)
    0x1C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A,
    // private flags
    0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(GG_UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.guid);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.header_->public_header.version_flag);
  EXPECT_FALSE(visitor_.header_->fec_flag);
  EXPECT_FALSE(visitor_.header_->entropy_flag);
  EXPECT_EQ(0, visitor_.header_->entropy_hash);
  EXPECT_EQ(GG_UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  // Now test framing boundaries
  for (size_t i = 0;
       i < GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                               PACKET_2BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
       ++i) {
    string expected_error;
    if (i < kGuidOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < GetSequenceNumberOffset(!kIncludeVersion)) {
      expected_error = "Unable to read GUID.";
    } else if (i < GetPrivateFlagsOffset(!kIncludeVersion,
                                         PACKET_2BYTE_SEQUENCE_NUMBER)) {
      expected_error = "Unable to read sequence number.";
    } else if (i < GetFecGroupOffset(!kIncludeVersion,
                                     PACKET_2BYTE_SEQUENCE_NUMBER)) {
      expected_error = "Unable to read private flags.";
    } else {
      expected_error = "Unable to read first fec protected packet offset.";
    }
    CheckProcessingFails(packet, i, expected_error, QUIC_INVALID_PACKET_HEADER);
  }
}

TEST_P(QuicFramerTest, PacketHeaderWith1ByteSequenceNumber) {
  QuicFramerPeer::SetLastSequenceNumber(&framer_,
                                        GG_UINT64_C(0x123456789ABA));

  unsigned char packet[] = {
    // public flags (8 byte guid and 1 byte sequence number)
    0x0C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC,
    // private flags
    0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(GG_UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.guid);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.header_->public_header.version_flag);
  EXPECT_FALSE(visitor_.header_->fec_flag);
  EXPECT_FALSE(visitor_.header_->entropy_flag);
  EXPECT_EQ(0, visitor_.header_->entropy_hash);
  EXPECT_EQ(GG_UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  // Now test framing boundaries
  for (size_t i = 0;
       i < GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                               PACKET_1BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
       ++i) {
    string expected_error;
    if (i < kGuidOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < GetSequenceNumberOffset(!kIncludeVersion)) {
      expected_error = "Unable to read GUID.";
    } else if (i < GetPrivateFlagsOffset(!kIncludeVersion,
                                         PACKET_1BYTE_SEQUENCE_NUMBER)) {
      expected_error = "Unable to read sequence number.";
    } else if (i < GetFecGroupOffset(!kIncludeVersion,
                                     PACKET_1BYTE_SEQUENCE_NUMBER)) {
      expected_error = "Unable to read private flags.";
    } else {
      expected_error = "Unable to read first fec protected packet offset.";
    }
    CheckProcessingFails(packet, i, expected_error, QUIC_INVALID_PACKET_HEADER);
  }
}

TEST_P(QuicFramerTest, InvalidPublicFlag) {
  unsigned char packet[] = {
    // public flags, unknown flag at bit 6
    0x40,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame count
    0x01,
    // frame type (stream frame)
    0x01,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // fin
    0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };
  CheckProcessingFails(packet,
                       arraysize(packet),
                       "Illegal public flags value.",
                       QUIC_INVALID_PACKET_HEADER);
};

TEST_P(QuicFramerTest, InvalidPublicFlagWithMatchingVersions) {
  // Set a specific version.
  framer_.set_version(QUIC_VERSION_7);

  unsigned char packet[] = {
    // public flags (8 byte guid and version flag and an unknown flag)
    0x4D,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // version tag
    'Q', '0', '0', '7',
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame count
    0x01,
    // frame type (stream frame)
    0x01,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // fin
    0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };
  CheckProcessingFails(packet,
                       arraysize(packet),
                       "Illegal public flags value.",
                       QUIC_INVALID_PACKET_HEADER);
};

TEST_P(QuicFramerTest, LargePublicFlagWithMismatchedVersions) {
  unsigned char packet[] = {
    // public flags (8 byte guid, version flag and an unknown flag)
    0x7D,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // version tag
    'Q', '0', '0', '0',
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (padding frame)
    0x07,
  };
  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(0, visitor_.frame_count_);
  EXPECT_EQ(1, visitor_.version_mismatch_);
};

TEST_P(QuicFramerTest, InvalidPrivateFlag) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x10,

    // frame count
    0x01,
    // frame type (stream frame)
    0x01,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // fin
    0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };
  CheckProcessingFails(packet,
                       arraysize(packet),
                       "Illegal private flags value.",
                       QUIC_INVALID_PACKET_HEADER);
};

TEST_P(QuicFramerTest, InvalidFECGroupOffset) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0x01, 0x00, 0x00, 0x00,
    0x00, 0x00,
    // private flags (fec group)
    0x02,
    // first fec protected packet offset
    0x10
  };
  CheckProcessingFails(packet,
                       arraysize(packet),
                       "First fec protected packet offset must be less "
                       "than the sequence number.",
                       QUIC_INVALID_PACKET_HEADER);
};

TEST_P(QuicFramerTest, PaddingFrame) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (padding frame)
    0x07,
    // Ignored data (which in this case is a stream frame)
    0x01,
    0x04, 0x03, 0x02, 0x01,
    0x01,
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    0x0c, 0x00,
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  ASSERT_EQ(0u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  // A packet with no frames is not acceptable.
  CheckProcessingFails(
      packet,
      GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                          PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
      "Unable to read frame type.", QUIC_INVALID_FRAME_DATA);
}

TEST_P(QuicFramerTest, StreamFrame) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (stream frame with fin)
    0xFE,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(static_cast<uint64>(0x01020304),
            visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(GG_UINT64_C(0xBA98FEDC32107654),
            visitor_.stream_frames_[0]->offset);
  EXPECT_EQ("hello world!", visitor_.stream_frames_[0]->data);

  // Now test framing boundaries
  for (size_t i = 0; i < GetMinStreamFrameSize(framer_.version()); ++i) {
    string expected_error;
    if (i < kQuicFrameTypeSize) {
      expected_error = "Unable to read frame type.";
    } else if (i < kQuicFrameTypeSize + kQuicMaxStreamIdSize) {
      expected_error = "Unable to read stream_id.";
    } else if (i < kQuicFrameTypeSize + kQuicMaxStreamIdSize) {
      expected_error = "Unable to read fin.";
    } else if (i < kQuicFrameTypeSize + kQuicMaxStreamIdSize +
               kQuicMaxStreamOffsetSize) {
      expected_error = "Unable to read offset.";
    } else {
      expected_error = "Unable to read frame data.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_FRAME_DATA);
  }
}

TEST_P(QuicFramerTest, StreamFrame3ByteStreamId) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (stream frame with fin)
    0xFC,
    // stream id
    0x04, 0x03, 0x02,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(static_cast<uint64>(0x00020304),
            visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(GG_UINT64_C(0xBA98FEDC32107654),
            visitor_.stream_frames_[0]->offset);
  EXPECT_EQ("hello world!", visitor_.stream_frames_[0]->data);

  // Now test framing boundaries
  const size_t stream_id_size = 3;
  for (size_t i = 0; i < GetMinStreamFrameSize(framer_.version()); ++i) {
    string expected_error;
    if (i < kQuicFrameTypeSize) {
      expected_error = "Unable to read frame type.";
    } else if (i < kQuicFrameTypeSize + stream_id_size) {
      expected_error = "Unable to read stream_id.";
    } else if (i < kQuicFrameTypeSize + stream_id_size - 1) {
      expected_error = "Unable to read fin.";
    } else if (i < kQuicFrameTypeSize + stream_id_size +
        kQuicMaxStreamOffsetSize) {
      expected_error = "Unable to read offset.";
    } else {
      expected_error = "Unable to read frame data.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
                                expected_error, QUIC_INVALID_FRAME_DATA);
  }
}

TEST_P(QuicFramerTest, StreamFrame2ByteStreamId) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (stream frame with fin)
    0xFA,
    // stream id
    0x04, 0x03,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(static_cast<uint64>(0x00000304),
            visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(GG_UINT64_C(0xBA98FEDC32107654),
            visitor_.stream_frames_[0]->offset);
  EXPECT_EQ("hello world!", visitor_.stream_frames_[0]->data);

  // Now test framing boundaries
  const size_t stream_id_size = 2;
  for (size_t i = 0; i < GetMinStreamFrameSize(framer_.version()); ++i) {
    string expected_error;
    if (i < kQuicFrameTypeSize) {
      expected_error = "Unable to read frame type.";
    } else if (i < kQuicFrameTypeSize + stream_id_size) {
      expected_error = "Unable to read stream_id.";
    } else if (i < kQuicFrameTypeSize + stream_id_size - 1) {
      expected_error = "Unable to read fin.";
    } else if (i < kQuicFrameTypeSize + stream_id_size +
        kQuicMaxStreamOffsetSize) {
      expected_error = "Unable to read offset.";
    } else {
      expected_error = "Unable to read frame data.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
                                expected_error, QUIC_INVALID_FRAME_DATA);
  }
}

TEST_P(QuicFramerTest, StreamFrame1ByteStreamId) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (stream frame with fin)
    0xF8,
    // stream id
    0x04,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(static_cast<uint64>(0x00000004),
            visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(GG_UINT64_C(0xBA98FEDC32107654),
            visitor_.stream_frames_[0]->offset);
  EXPECT_EQ("hello world!", visitor_.stream_frames_[0]->data);

  // Now test framing boundaries
  const size_t stream_id_size = 1;
  for (size_t i = 0; i < GetMinStreamFrameSize(framer_.version()); ++i) {
    string expected_error;
    if (i < kQuicFrameTypeSize) {
      expected_error = "Unable to read frame type.";
    } else if (i < kQuicFrameTypeSize + stream_id_size) {
      expected_error = "Unable to read stream_id.";
    } else if (i < kQuicFrameTypeSize + stream_id_size - 1) {
      expected_error = "Unable to read fin.";
    } else if (i < kQuicFrameTypeSize + stream_id_size +
        kQuicMaxStreamOffsetSize) {
      expected_error = "Unable to read offset.";
    } else {
      expected_error = "Unable to read frame data.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
                                expected_error, QUIC_INVALID_FRAME_DATA);
  }
}

TEST_P(QuicFramerTest, StreamFrameWithVersion) {
  // Set a specific version.
  framer_.set_version(QUIC_VERSION_7);

  unsigned char packet[] = {
    // public flags (version, 8 byte guid)
    0x3D,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // version tag
    'Q', '0', '0', '7',
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (stream frame with fin)
    0xFE,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(visitor_.header_.get()->public_header.version_flag);
  EXPECT_EQ(QUIC_VERSION_7, visitor_.header_.get()->public_header.versions[0]);
  EXPECT_TRUE(CheckDecryption(encrypted, kIncludeVersion));

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(static_cast<uint64>(0x01020304),
            visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(GG_UINT64_C(0xBA98FEDC32107654),
            visitor_.stream_frames_[0]->offset);
  EXPECT_EQ("hello world!", visitor_.stream_frames_[0]->data);

  // Now test framing boundaries
  for (size_t i = 0; i < GetMinStreamFrameSize(framer_.version()); ++i) {
    string expected_error;
    if (i < kQuicFrameTypeSize) {
      expected_error = "Unable to read frame type.";
    } else if (i < kQuicFrameTypeSize + kQuicMaxStreamIdSize) {
      expected_error = "Unable to read stream_id.";
    } else if (i < kQuicFrameTypeSize + kQuicMaxStreamIdSize +
               kQuicMaxStreamOffsetSize) {
      expected_error = "Unable to read offset.";
    } else {
      expected_error = "Unable to read frame data.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_GUID, kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_FRAME_DATA);
  }
}

TEST_P(QuicFramerTest, RejectPacket) {
  visitor_.accept_packet_ = false;

  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (stream frame with fin)
    0xFE,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  ASSERT_EQ(0u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
}

TEST_P(QuicFramerTest, RevivedStreamFrame) {
  unsigned char payload[] = {
    // frame type (stream frame with fin)
    0xFE,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = true;
  header.entropy_flag = true;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  // Do not encrypt the payload because the revived payload is post-encryption.
  EXPECT_TRUE(framer_.ProcessRevivedPacket(&header,
                                           StringPiece(AsChars(payload),
                                                       arraysize(payload))));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_EQ(1, visitor_.revived_packets_);
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(GG_UINT64_C(0xFEDCBA9876543210),
            visitor_.header_->public_header.guid);
  EXPECT_FALSE(visitor_.header_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.header_->public_header.version_flag);
  EXPECT_TRUE(visitor_.header_->fec_flag);
  EXPECT_TRUE(visitor_.header_->entropy_flag);
  EXPECT_EQ(1 << (header.packet_sequence_number % 8),
            visitor_.header_->entropy_hash);
  EXPECT_EQ(GG_UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(NOT_IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(0x00u, visitor_.header_->fec_group);

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(GG_UINT64_C(0x01020304), visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(GG_UINT64_C(0xBA98FEDC32107654),
            visitor_.stream_frames_[0]->offset);
  EXPECT_EQ("hello world!", visitor_.stream_frames_[0]->data);
}

TEST_P(QuicFramerTest, StreamFrameInFecGroup) {
  // Set a specific version.
  framer_.set_version(QUIC_VERSION_7);

  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x12, 0x34,
    // private flags (fec group)
    0x02,
    // first fec protected packet offset
    0x02,

    // frame type (stream frame with fin)
    0xFE,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));
  EXPECT_EQ(IN_FEC_GROUP, visitor_.header_->is_in_fec_group);
  EXPECT_EQ(GG_UINT64_C(0x341256789ABA),
            visitor_.header_->fec_group);
  const size_t fec_offset = GetStartOfFecProtectedData(
      PACKET_8BYTE_GUID, !kIncludeVersion, PACKET_6BYTE_SEQUENCE_NUMBER);
  EXPECT_EQ(
      string(AsChars(packet) + fec_offset, arraysize(packet) - fec_offset),
      visitor_.fec_protected_payload_);

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(GG_UINT64_C(0x01020304), visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(GG_UINT64_C(0xBA98FEDC32107654),
            visitor_.stream_frames_[0]->offset);
  EXPECT_EQ("hello world!", visitor_.stream_frames_[0]->data);
}

TEST_P(QuicFramerTest, AckFrame) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (ack frame)
    static_cast<unsigned char>(0x01),
    // entropy hash of sent packets till least awaiting - 1.
    0xAB,
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // entropy hash of all received packets.
    0xBA,
    // largest observed packet sequence number
    0xBF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // Infinite delta time.
    0xFF, 0xFF, 0xFF, 0xFF,
    // num missing packets
    0x01,
    // missing packet
    0xBE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(0xAB, frame.sent_info.entropy_hash);
  EXPECT_EQ(0xBA, frame.received_info.entropy_hash);
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABF), frame.received_info.largest_observed);
  ASSERT_EQ(1u, frame.received_info.missing_packets.size());
  SequenceNumberSet::const_iterator missing_iter =
      frame.received_info.missing_packets.begin();
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABE), *missing_iter);
  EXPECT_EQ(GG_UINT64_C(0x0123456789AA0), frame.sent_info.least_unacked);

  const size_t kSentEntropyOffset = kQuicFrameTypeSize;
  const size_t kLeastUnackedOffset = kSentEntropyOffset + kQuicEntropyHashSize;
  const size_t kReceivedEntropyOffset = kLeastUnackedOffset +
      PACKET_6BYTE_SEQUENCE_NUMBER;
  const size_t kLargestObservedOffset = kReceivedEntropyOffset +
      kQuicEntropyHashSize;
  const size_t kMissingDeltaTimeOffset = kLargestObservedOffset +
      PACKET_6BYTE_SEQUENCE_NUMBER;
  const size_t kNumMissingPacketOffset = kMissingDeltaTimeOffset +
      kQuicDeltaTimeLargestObservedSize;
  const size_t kMissingPacketsOffset = kNumMissingPacketOffset +
      kNumberOfMissingPacketsSize;
  // Now test framing boundaries
  const size_t missing_packets_size = 1 * PACKET_6BYTE_SEQUENCE_NUMBER;
  for (size_t i = 0;
       i < QuicFramer::GetMinAckFrameSize() + missing_packets_size; ++i) {
    string expected_error;
    if (i < kSentEntropyOffset) {
      expected_error = "Unable to read frame type.";
    } else if (i < kLeastUnackedOffset) {
      expected_error = "Unable to read entropy hash for sent packets.";
    } else if (i < kReceivedEntropyOffset) {
      expected_error = "Unable to read least unacked.";
    } else if (i < kLargestObservedOffset) {
      expected_error = "Unable to read entropy hash for received packets.";
    } else if (i < kMissingDeltaTimeOffset) {
      expected_error = "Unable to read largest observed.";
    } else if (i < kNumMissingPacketOffset) {
      expected_error = "Unable to read delta time largest observed.";
    } else if (i < kMissingPacketsOffset) {
      expected_error = "Unable to read num missing packets.";
    } else {
      expected_error = "Unable to read sequence number in missing packets.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_FRAME_DATA);
  }
}

TEST_P(QuicFramerTest, CongestionFeedbackFrameTCP) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (congestion feedback frame)
    0x03,
    // congestion feedback type (tcp)
    0x00,
    // ack_frame.feedback.tcp.accumulated_number_of_lost_packets
    0x01, 0x02,
    // ack_frame.feedback.tcp.receive_window
    0x03, 0x04,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.congestion_feedback_frames_.size());
  const QuicCongestionFeedbackFrame& frame =
      *visitor_.congestion_feedback_frames_[0];
  ASSERT_EQ(kTCP, frame.type);
  EXPECT_EQ(0x0201,
            frame.tcp.accumulated_number_of_lost_packets);
  EXPECT_EQ(0x4030u, frame.tcp.receive_window);

  // Now test framing boundaries
  for (size_t i = 0; i < 6; ++i) {
    string expected_error;
    if (i < 1) {
      expected_error = "Unable to read frame type.";
    } else if (i < 2) {
      expected_error = "Unable to read congestion feedback type.";
    } else if (i < 4) {
      expected_error = "Unable to read accumulated number of lost packets.";
    } else if (i < 6) {
      expected_error = "Unable to read receive window.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_FRAME_DATA);
  }
}

TEST_P(QuicFramerTest, CongestionFeedbackFrameInterArrival) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (congestion feedback frame)
    0x03,
    // congestion feedback type (inter arrival)
    0x01,
    // accumulated_number_of_lost_packets
    0x02, 0x03,
    // num received packets
    0x03,
    // lowest sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // receive time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0x07,
    // sequence delta
    0x01, 0x00,
    // time delta
    0x01, 0x00, 0x00, 0x00,
    // sequence delta (skip one packet)
    0x03, 0x00,
    // time delta
    0x02, 0x00, 0x00, 0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.congestion_feedback_frames_.size());
  const QuicCongestionFeedbackFrame& frame =
      *visitor_.congestion_feedback_frames_[0];
  ASSERT_EQ(kInterArrival, frame.type);
  EXPECT_EQ(0x0302, frame.inter_arrival.
            accumulated_number_of_lost_packets);
  ASSERT_EQ(3u, frame.inter_arrival.received_packet_times.size());
  TimeMap::const_iterator iter =
      frame.inter_arrival.received_packet_times.begin();
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABA), iter->first);
  EXPECT_EQ(GG_INT64_C(0x07E1D2C3B4A59687),
            iter->second.Subtract(start_).ToMicroseconds());
  ++iter;
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABB), iter->first);
  EXPECT_EQ(GG_INT64_C(0x07E1D2C3B4A59688),
            iter->second.Subtract(start_).ToMicroseconds());
  ++iter;
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABD), iter->first);
  EXPECT_EQ(GG_INT64_C(0x07E1D2C3B4A59689),
            iter->second.Subtract(start_).ToMicroseconds());

  // Now test framing boundaries
  for (size_t i = 0; i < 31; ++i) {
    string expected_error;
    if (i < 1) {
      expected_error = "Unable to read frame type.";
    } else if (i < 2) {
      expected_error = "Unable to read congestion feedback type.";
    } else if (i < 4) {
      expected_error = "Unable to read accumulated number of lost packets.";
    } else if (i < 5) {
      expected_error = "Unable to read num received packets.";
    } else if (i < 11) {
      expected_error = "Unable to read smallest received.";
    } else if (i < 19) {
      expected_error = "Unable to read time received.";
    } else if (i < 21) {
      expected_error = "Unable to read sequence delta in received packets.";
    } else if (i < 25) {
      expected_error = "Unable to read time delta in received packets.";
    } else if (i < 27) {
      expected_error = "Unable to read sequence delta in received packets.";
    } else if (i < 31) {
      expected_error = "Unable to read time delta in received packets.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_FRAME_DATA);
  }
}

TEST_P(QuicFramerTest, CongestionFeedbackFrameFixRate) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (congestion feedback frame)
    0x03,
    // congestion feedback type (fix rate)
    0x02,
    // bitrate_in_bytes_per_second;
    0x01, 0x02, 0x03, 0x04,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.congestion_feedback_frames_.size());
  const QuicCongestionFeedbackFrame& frame =
      *visitor_.congestion_feedback_frames_[0];
  ASSERT_EQ(kFixRate, frame.type);
  EXPECT_EQ(static_cast<uint32>(0x04030201),
            frame.fix_rate.bitrate.ToBytesPerSecond());

  // Now test framing boundaries
  for (size_t i = 0; i < 6; ++i) {
    string expected_error;
    if (i < 1) {
      expected_error = "Unable to read frame type.";
    } else if (i < 2) {
      expected_error = "Unable to read congestion feedback type.";
    } else if (i < 6) {
      expected_error = "Unable to read bitrate.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_FRAME_DATA);
  }
}


TEST_P(QuicFramerTest, CongestionFeedbackFrameInvalidFeedback) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (congestion feedback frame)
    0x03,
    // congestion feedback type (invalid)
    0x03,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(encrypted));
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));
  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
}

TEST_P(QuicFramerTest, RstStreamFrame) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (rst stream frame)
    static_cast<unsigned char>(0x27),
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // error code
    0x01, 0x00, 0x00, 0x00,

    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(GG_UINT64_C(0x01020304), visitor_.rst_stream_frame_.stream_id);
  EXPECT_EQ(0x01, visitor_.rst_stream_frame_.error_code);
  EXPECT_EQ("because I can", visitor_.rst_stream_frame_.error_details);

  // Now test framing boundaries
  for (size_t i = 2; i < 24; ++i) {
    string expected_error;
    if (i < kQuicFrameTypeSize + kQuicMaxStreamIdSize) {
      expected_error = "Unable to read stream_id.";
    } else if (i < kQuicFrameTypeSize + kQuicMaxStreamIdSize +
               kQuicErrorCodeSize) {
      expected_error = "Unable to read rst stream error code.";
    } else {
      expected_error = "Unable to read rst stream error details.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_RST_STREAM_DATA);
  }
}

TEST_P(QuicFramerTest, ConnectionCloseFrame) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (connection close frame)
    static_cast<unsigned char>(0x2F),
    // error code
    0x11, 0x00, 0x00, 0x00,

    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',

    // Ack frame.
    // entropy hash of sent packets till least awaiting - 1.
    0xBF,
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // entropy hash of all received packets.
    0xEB,
    // largest observed packet sequence number
    0xBF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // Infinite delta time.
    0xFF, 0xFF, 0xFF, 0xFF,
    // num missing packets
    0x01,
    // missing packet
    0xBE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(0x11, visitor_.connection_close_frame_.error_code);
  EXPECT_EQ("because I can", visitor_.connection_close_frame_.error_details);

  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(0xBF, frame.sent_info.entropy_hash);
  EXPECT_EQ(GG_UINT64_C(0x0123456789AA0), frame.sent_info.least_unacked);
  EXPECT_EQ(0xEB, frame.received_info.entropy_hash);
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABF), frame.received_info.largest_observed);
  ASSERT_EQ(1u, frame.received_info.missing_packets.size());
  SequenceNumberSet::const_iterator missing_iter =
      frame.received_info.missing_packets.begin();
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABE), *missing_iter);

  // Now test framing boundaries
  for (size_t i = kQuicFrameTypeSize;
       i < QuicFramer::GetMinConnectionCloseFrameSize() -
           QuicFramer::GetMinAckFrameSize(); ++i) {
    string expected_error;
    if (i < kQuicFrameTypeSize + kQuicErrorCodeSize) {
      expected_error = "Unable to read connection close error code.";
    } else {
      expected_error = "Unable to read connection close error details.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_CONNECTION_CLOSE_DATA);
  }
}

TEST_P(QuicFramerTest, GoAwayFrame) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (go away frame)
    static_cast<unsigned char>(0x37),
    // error code
    0x09, 0x00, 0x00, 0x00,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(GG_UINT64_C(0x01020304),
            visitor_.goaway_frame_.last_good_stream_id);
  EXPECT_EQ(0x9, visitor_.goaway_frame_.error_code);
  EXPECT_EQ("because I can", visitor_.goaway_frame_.reason_phrase);

  const size_t reason_size = arraysize("because I can") - 1;
  // Now test framing boundaries
  for (size_t i = kQuicFrameTypeSize;
       i < QuicFramer::GetMinGoAwayFrameSize() + reason_size; ++i) {
    string expected_error;
    if (i < kQuicFrameTypeSize + kQuicErrorCodeSize) {
      expected_error = "Unable to read go away error code.";
    } else if (i < kQuicFrameTypeSize + kQuicErrorCodeSize +
               kQuicMaxStreamIdSize) {
      expected_error = "Unable to read last good stream id.";
    } else {
      expected_error = "Unable to read goaway reason.";
    }
    CheckProcessingFails(
        packet,
        i + GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP),
        expected_error, QUIC_INVALID_GOAWAY_DATA);
  }
}

TEST_P(QuicFramerTest, PublicResetPacket) {
  unsigned char packet[] = {
    // public flags (public reset, 8 byte guid)
    0x3E,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // nonce proof
    0x89, 0x67, 0x45, 0x23,
    0x01, 0xEF, 0xCD, 0xAB,
    // rejected sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  ASSERT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.public_reset_packet_.get());
  EXPECT_EQ(GG_UINT64_C(0xFEDCBA9876543210),
            visitor_.public_reset_packet_->public_header.guid);
  EXPECT_TRUE(visitor_.public_reset_packet_->public_header.reset_flag);
  EXPECT_FALSE(visitor_.public_reset_packet_->public_header.version_flag);
  EXPECT_EQ(GG_UINT64_C(0xABCDEF0123456789),
            visitor_.public_reset_packet_->nonce_proof);
  EXPECT_EQ(GG_UINT64_C(0x123456789ABC),
            visitor_.public_reset_packet_->rejected_sequence_number);

  // Now test framing boundaries
  for (size_t i = 0; i < GetPublicResetPacketSize(); ++i) {
    string expected_error;
    DLOG(INFO) << "iteration: " << i;
    if (i < kGuidOffset) {
      expected_error = "Unable to read public flags.";
      CheckProcessingFails(packet, i, expected_error,
                           QUIC_INVALID_PACKET_HEADER);
    } else if (i < kPublicResetPacketNonceProofOffset) {
      expected_error = "Unable to read GUID.";
      CheckProcessingFails(packet, i, expected_error,
                           QUIC_INVALID_PACKET_HEADER);
    } else if (i < kPublicResetPacketRejectedSequenceNumberOffset) {
      expected_error = "Unable to read nonce proof.";
      CheckProcessingFails(packet, i, expected_error,
                           QUIC_INVALID_PUBLIC_RST_PACKET);
    } else {
      expected_error = "Unable to read rejected sequence number.";
      CheckProcessingFails(packet, i, expected_error,
                           QUIC_INVALID_PUBLIC_RST_PACKET);
    }
  }
}

TEST_P(QuicFramerTest, VersionNegotiationPacket) {
  // Set a specific version.
  framer_.set_version(QUIC_VERSION_7);

  unsigned char packet[] = {
    // public flags (version, 8 byte guid)
    0x3D,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // version tag
    'Q', '0', '0', '7',
    'Q', '2', '.', '0',
  };

  QuicFramerPeer::SetIsServer(&framer_, false);

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  ASSERT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.version_negotiation_packet_.get());
  EXPECT_EQ(2u, visitor_.version_negotiation_packet_->versions.size());
  EXPECT_EQ(QUIC_VERSION_7,
            visitor_.version_negotiation_packet_->versions[0]);

  for (size_t i = 0; i <= kPublicFlagsSize + PACKET_8BYTE_GUID; ++i) {
    string expected_error;
    QuicErrorCode error_code = QUIC_INVALID_PACKET_HEADER;
    if (i < kGuidOffset) {
      expected_error = "Unable to read public flags.";
    } else if (i < kVersionOffset) {
      expected_error = "Unable to read GUID.";
    } else {
      expected_error = "Unable to read supported version in negotiation.";
      error_code = QUIC_INVALID_VERSION_NEGOTIATION_PACKET;
    }
    CheckProcessingFails(packet, i, expected_error, error_code);
  }
}

TEST_P(QuicFramerTest, FecPacket) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (fec group & FEC)
    0x06,
    // first fec protected packet offset
    0x01,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(CheckDecryption(encrypted, !kIncludeVersion));

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  ASSERT_EQ(1, visitor_.fec_count_);
  const QuicFecData& fec_data = *visitor_.fec_data_[0];
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABB), fec_data.fec_group);
  EXPECT_EQ("abcdefghijklmnop", fec_data.redundancy);
}

TEST_P(QuicFramerTest, BuildPaddingFramePacket) {
  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicPaddingFrame padding_frame;

  QuicFrames frames;
  frames.push_back(QuicFrame(&padding_frame));

  unsigned char packet[kMaxPacketSize] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (padding frame)
    static_cast<unsigned char>(0x07),
  };

  uint64 header_size =
      GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                          PACKET_6BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
  memset(packet + header_size + 1, 0x00, kMaxPacketSize - header_size - 1);

  scoped_ptr<QuicPacket> data(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, Build4ByteSequenceNumberPaddingFramePacket) {
  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.public_header.sequence_number_length = PACKET_4BYTE_SEQUENCE_NUMBER;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicPaddingFrame padding_frame;

  QuicFrames frames;
  frames.push_back(QuicFrame(&padding_frame));

  unsigned char packet[kMaxPacketSize] = {
    // public flags (8 byte guid and 4 byte sequence number)
    0x2C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    // private flags
    0x00,

    // frame type (padding frame)
    static_cast<unsigned char>(0x07),
  };

  uint64 header_size =
      GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                          PACKET_4BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
  memset(packet + header_size + 1, 0x00, kMaxPacketSize - header_size - 1);

  scoped_ptr<QuicPacket> data(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, Build2ByteSequenceNumberPaddingFramePacket) {
  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.public_header.sequence_number_length = PACKET_2BYTE_SEQUENCE_NUMBER;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicPaddingFrame padding_frame;

  QuicFrames frames;
  frames.push_back(QuicFrame(&padding_frame));

  unsigned char packet[kMaxPacketSize] = {
    // public flags (8 byte guid and 2 byte sequence number)
    0x1C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A,
    // private flags
    0x00,

    // frame type (padding frame)
    static_cast<unsigned char>(0x07),
  };

  uint64 header_size =
      GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                          PACKET_2BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
  memset(packet + header_size + 1, 0x00, kMaxPacketSize - header_size - 1);

  scoped_ptr<QuicPacket> data(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, Build1ByteSequenceNumberPaddingFramePacket) {
  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.public_header.sequence_number_length = PACKET_1BYTE_SEQUENCE_NUMBER;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicPaddingFrame padding_frame;

  QuicFrames frames;
  frames.push_back(QuicFrame(&padding_frame));

  unsigned char packet[kMaxPacketSize] = {
    // public flags (8 byte guid and 1 byte sequence number)
    0x0C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC,
    // private flags
    0x00,

    // frame type (padding frame)
    static_cast<unsigned char>(0x07),
  };

  uint64 header_size =
      GetPacketHeaderSize(PACKET_8BYTE_GUID, !kIncludeVersion,
                          PACKET_1BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP);
  memset(packet + header_size + 1, 0x00, kMaxPacketSize - header_size - 1);

  scoped_ptr<QuicPacket> data(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildStreamFramePacket) {
  // Set a specific version.
  framer_.set_version(QUIC_VERSION_7);

  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = GG_UINT64_C(0x77123456789ABC);
  header.fec_group = 0;

  QuicStreamFrame stream_frame;
  stream_frame.stream_id = 0x01020304;
  stream_frame.fin = true;
  stream_frame.offset = GG_UINT64_C(0xBA98FEDC32107654);
  stream_frame.data = "hello world!";

  QuicFrames frames;
  frames.push_back(QuicFrame(&stream_frame));

  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (entropy)
    0x01,

    // frame type (stream frame with fin and no length)
    0xBE,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  scoped_ptr<QuicPacket> data(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildStreamFramePacketWithVersionFlag) {
  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = true;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = GG_UINT64_C(0x77123456789ABC);
  header.fec_group = 0;

  QuicStreamFrame stream_frame;
  stream_frame.stream_id = 0x01020304;
  stream_frame.fin = true;
  stream_frame.offset = GG_UINT64_C(0xBA98FEDC32107654);
  stream_frame.data = "hello world!";

  QuicFrames frames;
  frames.push_back(QuicFrame(&stream_frame));

  // Set a specific version.
  framer_.set_version(QUIC_VERSION_7);
  unsigned char packet[] = {
    // public flags (version, 8 byte guid)
    0x3D,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // version tag
    'Q', '0', '0', '7',
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (entropy)
    0x01,

    // frame type (stream frame with fin and no length)
    0xBE,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicFramerPeer::SetIsServer(&framer_, false);
  scoped_ptr<QuicPacket> data(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildVersionNegotiationPacket) {
  QuicPacketPublicHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.reset_flag = false;
  header.version_flag = true;

  unsigned char packet[] = {
    // public flags (version, 8 byte guid)
    0x3D,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // version tag
    'Q', '0', '0', '7',
  };

  QuicVersionVector versions;
  versions.push_back(QUIC_VERSION_7);
  scoped_ptr<QuicEncryptedPacket> data(
      framer_.BuildVersionNegotiationPacket(header, versions));

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildAckFramePacket) {
  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicAckFrame ack_frame;
  ack_frame.received_info.entropy_hash = 0x43;
  ack_frame.received_info.largest_observed = GG_UINT64_C(0x770123456789ABF);
  ack_frame.received_info.delta_time_largest_observed = QuicTime::Delta::Zero();
  ack_frame.received_info.missing_packets.insert(
      GG_UINT64_C(0x770123456789ABE));
  ack_frame.sent_info.entropy_hash = 0x14;
  ack_frame.sent_info.least_unacked = GG_UINT64_C(0x770123456789AA0);

  QuicFrames frames;
  frames.push_back(QuicFrame(&ack_frame));

  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (entropy)
    0x01,

    // frame type (ack frame)
    static_cast<unsigned char>(0x01),
    // entropy hash of sent packets till least awaiting - 1.
    0x14,
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // entropy hash of all received packets.
    0x43,
    // largest observed packet sequence number
    0xBF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // Zero delta time.
    0x0, 0x0, 0x0, 0x0,
    // num missing packets
    0x01,
    // missing packet
    0xBE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
  };

  scoped_ptr<QuicPacket> data(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildCongestionFeedbackFramePacketTCP) {
  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicCongestionFeedbackFrame congestion_feedback_frame;
  congestion_feedback_frame.type = kTCP;
  congestion_feedback_frame.tcp.accumulated_number_of_lost_packets = 0x0201;
  congestion_feedback_frame.tcp.receive_window = 0x4030;

  QuicFrames frames;
  frames.push_back(QuicFrame(&congestion_feedback_frame));

  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (congestion feedback frame)
    0x03,
    // congestion feedback type (TCP)
    0x00,
    // accumulated number of lost packets
    0x01, 0x02,
    // TCP receive window
    0x03, 0x04,
  };

  scoped_ptr<QuicPacket> data(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildCongestionFeedbackFramePacketInterArrival) {
  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicCongestionFeedbackFrame frame;
  frame.type = kInterArrival;
  frame.inter_arrival.accumulated_number_of_lost_packets = 0x0302;
  frame.inter_arrival.received_packet_times.insert(
      make_pair(GG_UINT64_C(0x0123456789ABA),
                start_.Add(QuicTime::Delta::FromMicroseconds(
                    GG_UINT64_C(0x07E1D2C3B4A59687)))));
  frame.inter_arrival.received_packet_times.insert(
      make_pair(GG_UINT64_C(0x0123456789ABB),
                start_.Add(QuicTime::Delta::FromMicroseconds(
                    GG_UINT64_C(0x07E1D2C3B4A59688)))));
  frame.inter_arrival.received_packet_times.insert(
      make_pair(GG_UINT64_C(0x0123456789ABD),
                start_.Add(QuicTime::Delta::FromMicroseconds(
                    GG_UINT64_C(0x07E1D2C3B4A59689)))));
  QuicFrames frames;
  frames.push_back(QuicFrame(&frame));

  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (congestion feedback frame)
    0x03,
    // congestion feedback type (inter arrival)
    0x01,
    // accumulated_number_of_lost_packets
    0x02, 0x03,
    // num received packets
    0x03,
    // lowest sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // receive time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0x07,
    // sequence delta
    0x01, 0x00,
    // time delta
    0x01, 0x00, 0x00, 0x00,
    // sequence delta (skip one packet)
    0x03, 0x00,
    // time delta
    0x02, 0x00, 0x00, 0x00,
  };

  scoped_ptr<QuicPacket> data(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildCongestionFeedbackFramePacketFixRate) {
  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicCongestionFeedbackFrame congestion_feedback_frame;
  congestion_feedback_frame.type = kFixRate;
  congestion_feedback_frame.fix_rate.bitrate
      = QuicBandwidth::FromBytesPerSecond(0x04030201);

  QuicFrames frames;
  frames.push_back(QuicFrame(&congestion_feedback_frame));

  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (congestion feedback frame)
    0x03,
    // congestion feedback type (fix rate)
    0x02,
    // bitrate_in_bytes_per_second;
    0x01, 0x02, 0x03, 0x04,
  };

  scoped_ptr<QuicPacket> data(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildCongestionFeedbackFramePacketInvalidFeedback) {
  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicCongestionFeedbackFrame congestion_feedback_frame;
  congestion_feedback_frame.type =
      static_cast<CongestionFeedbackType>(kFixRate + 1);

  QuicFrames frames;
  frames.push_back(QuicFrame(&congestion_feedback_frame));

  scoped_ptr<QuicPacket> data(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(data == NULL);
}

TEST_P(QuicFramerTest, BuildRstFramePacket) {
  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicRstStreamFrame rst_frame;
  rst_frame.stream_id = 0x01020304;
  rst_frame.error_code = static_cast<QuicRstStreamErrorCode>(0x05060708);
  rst_frame.error_details = "because I can";

  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (rst stream frame)
    static_cast<unsigned char>(0x27),
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // error code
    0x08, 0x07, 0x06, 0x05,
    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  QuicFrames frames;
  frames.push_back(QuicFrame(&rst_frame));

  scoped_ptr<QuicPacket> data(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildCloseFramePacket) {
  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicConnectionCloseFrame close_frame;
  close_frame.error_code = static_cast<QuicErrorCode>(0x05060708);
  close_frame.error_details = "because I can";

  QuicAckFrame* ack_frame = &close_frame.ack_frame;
  ack_frame->received_info.entropy_hash = 0x43;
  ack_frame->received_info.largest_observed = GG_UINT64_C(0x0123456789ABF);
  ack_frame->received_info.missing_packets.insert(GG_UINT64_C(0x0123456789ABE));
  ack_frame->sent_info.entropy_hash = 0xE0;
  ack_frame->sent_info.least_unacked = GG_UINT64_C(0x0123456789AA0);

  QuicFrames frames;
  frames.push_back(QuicFrame(&close_frame));

  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (entropy)
    0x01,

    // frame type (connection close frame)
    static_cast<unsigned char>(0x2F),
    // error code
    0x08, 0x07, 0x06, 0x05,
    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',

    // Ack frame.
    // entropy hash of sent packets till least awaiting - 1.
    0xE0,
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // entropy hash of all received packets.
    0x43,
    // largest observed packet sequence number
    0xBF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // Infinite delta time.
    0xFF, 0xFF, 0xFF, 0xFF,
    // num missing packets
    0x01,
    // missing packet
    0xBE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
  };

  scoped_ptr<QuicPacket> data(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildGoAwayPacket) {
  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicGoAwayFrame goaway_frame;
  goaway_frame.error_code = static_cast<QuicErrorCode>(0x05060708);
  goaway_frame.last_good_stream_id = 0x01020304;
  goaway_frame.reason_phrase = "because I can";

  QuicFrames frames;
  frames.push_back(QuicFrame(&goaway_frame));

  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags(entropy)
    0x01,

    // frame type (go away frame)
    static_cast<unsigned char>(0x37),
    // error code
    0x08, 0x07, 0x06, 0x05,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  scoped_ptr<QuicPacket> data(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildPublicResetPacket) {
  QuicPublicResetPacket reset_packet;
  reset_packet.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  reset_packet.public_header.reset_flag = true;
  reset_packet.public_header.version_flag = false;
  reset_packet.rejected_sequence_number = GG_UINT64_C(0x123456789ABC);
  reset_packet.nonce_proof = GG_UINT64_C(0xABCDEF0123456789);

  unsigned char packet[] = {
    // public flags (public reset, 8 byte GUID)
    0x3E,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // nonce proof
    0x89, 0x67, 0x45, 0x23,
    0x01, 0xEF, 0xCD, 0xAB,
    // rejected sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
  };

  scoped_ptr<QuicEncryptedPacket> data(
      framer_.BuildPublicResetPacket(reset_packet));
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, BuildFecPacket) {
  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = true;
  header.entropy_flag = true;
  header.packet_sequence_number = (GG_UINT64_C(0x123456789ABC));
  header.is_in_fec_group = IN_FEC_GROUP;
  header.fec_group = GG_UINT64_C(0x123456789ABB);;

  QuicFecData fec_data;
  fec_data.fec_group = 1;
  fec_data.redundancy = "abcdefghijklmnop";

  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (entropy & fec group & fec packet)
    0x07,
    // first fec protected packet offset
    0x01,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  scoped_ptr<QuicPacket> data(
      framer_.BuildFecPacket(header, fec_data).packet);
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_P(QuicFramerTest, EncryptPacket) {
  QuicPacketSequenceNumber sequence_number = GG_UINT64_C(0x123456789ABC);
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (fec group & fec packet)
    0x06,
    // first fec protected packet offset
    0x01,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  scoped_ptr<QuicPacket> raw(
      QuicPacket::NewDataPacket(AsChars(packet), arraysize(packet), false,
                                PACKET_8BYTE_GUID, !kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER));
  scoped_ptr<QuicEncryptedPacket> encrypted(
      framer_.EncryptPacket(ENCRYPTION_NONE, sequence_number, *raw));

  ASSERT_TRUE(encrypted.get() != NULL);
  EXPECT_TRUE(CheckEncryption(sequence_number, raw.get()));
}

TEST_P(QuicFramerTest, EncryptPacketWithVersionFlag) {
  QuicPacketSequenceNumber sequence_number = GG_UINT64_C(0x123456789ABC);
  unsigned char packet[] = {
    // public flags (version, 8 byte guid)
    0x3D,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // version tag
    'Q', '.', '1', '0',
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (fec group & fec flags)
    0x06,
    // first fec protected packet offset
    0x01,

    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  scoped_ptr<QuicPacket> raw(
      QuicPacket::NewDataPacket(AsChars(packet), arraysize(packet), false,
                                PACKET_8BYTE_GUID, kIncludeVersion,
                                PACKET_6BYTE_SEQUENCE_NUMBER));
  scoped_ptr<QuicEncryptedPacket> encrypted(
      framer_.EncryptPacket(ENCRYPTION_NONE, sequence_number, *raw));

  ASSERT_TRUE(encrypted.get() != NULL);
  EXPECT_TRUE(CheckEncryption(sequence_number, raw.get()));
}

// TODO(rch): re-enable after https://codereview.chromium.org/11820005/
// lands.  Currently this is causing valgrind problems, but it should be
// fixed in the followup CL.
TEST_P(QuicFramerTest, DISABLED_CalculateLargestReceived) {
  SequenceNumberSet missing;
  missing.insert(1);
  missing.insert(5);
  missing.insert(7);

  // These two we just walk to the next gap, and return the largest seen.
  EXPECT_EQ(4u, QuicFramer::CalculateLargestObserved(missing, missing.find(1)));
  EXPECT_EQ(6u, QuicFramer::CalculateLargestObserved(missing, missing.find(5)));

  missing.insert(2);
  // For 1, we can't go forward as 2 would be implicitly acked so we return the
  // largest missing packet.
  EXPECT_EQ(1u, QuicFramer::CalculateLargestObserved(missing, missing.find(1)));
  // For 2, we've seen 3 and 4, so can admit to a largest observed.
  EXPECT_EQ(4u, QuicFramer::CalculateLargestObserved(missing, missing.find(2)));
}

// TODO(rch) enable after landing the revised truncation CL.
TEST_P(QuicFramerTest, DISABLED_Truncation) {
  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = false;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicConnectionCloseFrame close_frame;
  QuicAckFrame* ack_frame = &close_frame.ack_frame;
  close_frame.error_code = static_cast<QuicErrorCode>(0x05);
  close_frame.error_details = "because I can";
  ack_frame->received_info.largest_observed = 201;
  ack_frame->sent_info.least_unacked = 0;
  for (uint64 i = 1; i < ack_frame->received_info.largest_observed; ++i) {
    ack_frame->received_info.missing_packets.insert(i);
  }

  // Create a packet with just the ack
  QuicFrame frame;
  frame.type = ACK_FRAME;
  frame.ack_frame = ack_frame;
  QuicFrames frames;
  frames.push_back(frame);

  scoped_ptr<QuicPacket> raw_ack_packet(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(raw_ack_packet != NULL);

  scoped_ptr<QuicEncryptedPacket> ack_packet(
      framer_.EncryptPacket(ENCRYPTION_NONE, header.packet_sequence_number,
                            *raw_ack_packet));

  // Create a packet with just connection close.
  frames.clear();
  frame.type = CONNECTION_CLOSE_FRAME;
  frame.connection_close_frame = &close_frame;
  frames.push_back(frame);

  scoped_ptr<QuicPacket> raw_close_packet(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(raw_close_packet != NULL);

  scoped_ptr<QuicEncryptedPacket> close_packet(
      framer_.EncryptPacket(ENCRYPTION_NONE, header.packet_sequence_number,
                            *raw_close_packet));

  // Now make sure we can turn our ack packet back into an ack frame
  ASSERT_TRUE(framer_.ProcessPacket(*ack_packet));

  // And do the same for the close frame.
  ASSERT_TRUE(framer_.ProcessPacket(*close_packet));
}

TEST_P(QuicFramerTest, CleanTruncation) {
  QuicPacketHeader header;
  header.public_header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.public_header.reset_flag = false;
  header.public_header.version_flag = false;
  header.fec_flag = false;
  header.entropy_flag = true;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.fec_group = 0;

  QuicConnectionCloseFrame close_frame;
  QuicAckFrame* ack_frame = &close_frame.ack_frame;
  close_frame.error_code = static_cast<QuicErrorCode>(0x05);
  close_frame.error_details = "because I can";
  ack_frame->received_info.largest_observed = 201;
  ack_frame->sent_info.least_unacked = 0;
  for (uint64 i = 1; i < ack_frame->received_info.largest_observed; ++i) {
    ack_frame->received_info.missing_packets.insert(i);
  }

  // Create a packet with just the ack
  QuicFrame frame;
  frame.type = ACK_FRAME;
  frame.ack_frame = ack_frame;
  QuicFrames frames;
  frames.push_back(frame);

  scoped_ptr<QuicPacket> raw_ack_packet(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(raw_ack_packet != NULL);

  scoped_ptr<QuicEncryptedPacket> ack_packet(
      framer_.EncryptPacket(ENCRYPTION_NONE, header.packet_sequence_number,
                            *raw_ack_packet));

  // Create a packet with just connection close.
  frames.clear();
  frame.type = CONNECTION_CLOSE_FRAME;
  frame.connection_close_frame = &close_frame;
  frames.push_back(frame);

  scoped_ptr<QuicPacket> raw_close_packet(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(raw_close_packet != NULL);

  scoped_ptr<QuicEncryptedPacket> close_packet(
      framer_.EncryptPacket(ENCRYPTION_NONE, header.packet_sequence_number,
                            *raw_close_packet));

  // Now make sure we can turn our ack packet back into an ack frame
  ASSERT_TRUE(framer_.ProcessPacket(*ack_packet));

  // And do the same for the close frame.
  ASSERT_TRUE(framer_.ProcessPacket(*close_packet));

  // Test for clean truncation of the ack by comparing the length of the
  // original packets to the re-serialized packets.
  frames.clear();
  frame.type = ACK_FRAME;
  frame.ack_frame = visitor_.ack_frames_[0];
  frames.push_back(frame);

  size_t original_raw_length = raw_ack_packet->length();
  raw_ack_packet.reset(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(raw_ack_packet != NULL);
  EXPECT_EQ(original_raw_length, raw_ack_packet->length());

  frames.clear();
  frame.type = CONNECTION_CLOSE_FRAME;
  frame.connection_close_frame = &visitor_.connection_close_frame_;
  frames.push_back(frame);

  original_raw_length = raw_close_packet->length();
  raw_close_packet.reset(
      framer_.BuildUnsizedDataPacket(header, frames).packet);
  ASSERT_TRUE(raw_ack_packet != NULL);
  EXPECT_EQ(original_raw_length, raw_close_packet->length());
}

TEST_P(QuicFramerTest, EntropyFlagTest) {
  // Set a specific version.
  framer_.set_version(QUIC_VERSION_7);

  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (Entropy)
    0x01,

    // frame type (stream frame with fin and no length)
    0xBE,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(visitor_.header_->entropy_flag);
  EXPECT_EQ(1 << 4, visitor_.header_->entropy_hash);
  EXPECT_FALSE(visitor_.header_->fec_flag);
};

TEST_P(QuicFramerTest, FecEntropyTest) {
  // Set a specific version.
  framer_.set_version(QUIC_VERSION_7);

  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags (Entropy & fec group & FEC)
    0x07,
    // first fec protected packet offset
    0xFF,

    // frame type (stream frame with fin and no length)
    0xBE,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_TRUE(visitor_.header_->fec_flag);
  EXPECT_TRUE(visitor_.header_->entropy_flag);
  EXPECT_EQ(1 << 4, visitor_.header_->entropy_hash);
};

TEST_P(QuicFramerTest, StopPacketProcessing) {
  // Set a specific version.
  framer_.set_version(QUIC_VERSION_7);

  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // Entropy
    0x01,

    // frame type (stream frame with fin)
    0xFE,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',

    // frame type (ack frame)
    0x02,
    // entropy hash of sent packets till least awaiting - 1.
    0x14,
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // entropy hash of all received packets.
    0x43,
    // largest observed packet sequence number
    0xBF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num missing packets
    0x01,
    // missing packet
    0xBE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
  };

  MockFramerVisitor visitor;
  framer_.set_visitor(&visitor);
  EXPECT_CALL(visitor, OnPacket());
  EXPECT_CALL(visitor, OnPacketHeader(_));
  EXPECT_CALL(visitor, OnStreamFrame(_)).WillOnce(Return(false));
  EXPECT_CALL(visitor, OnAckFrame(_)).Times(0);
  EXPECT_CALL(visitor, OnPacketComplete());

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
}

TEST_P(QuicFramerTest, ConnectionCloseWithInvalidAck) {
  unsigned char packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00,

    // frame type (connection close frame)
    static_cast<unsigned char>(0x2F),
    // error code
    0x11, 0x00, 0x00, 0x00,
    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',

    // Ack frame.
    // entropy hash of sent packets till least awaiting - 1.
    0xE0,
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // entropy hash of all received packets.
    0x43,
    // largest observed packet sequence number
    0xBF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // Infinite delta time.
    0xFF, 0xFF, 0xFF, 0xFF,
    // num missing packets
    0x01,
    // missing packet
    0xBE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
  };

  MockFramerVisitor visitor;
  framer_.set_visitor(&visitor);
  EXPECT_CALL(visitor, OnPacket());
  EXPECT_CALL(visitor, OnPacketHeader(_));
  EXPECT_CALL(visitor, OnAckFrame(_)).WillOnce(Return(false));
  EXPECT_CALL(visitor, OnConnectionCloseFrame(_)).Times(0);
  EXPECT_CALL(visitor, OnPacketComplete());

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(encrypted));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
}

}  // namespace test
}  // namespace net
