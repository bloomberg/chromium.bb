// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/quic_packets.h"

#include "net/quic/core/quic_utils.h"
#include "net/quic/core/quic_versions.h"
#include "net/quic/platform/api/quic_flags.h"
#include "net/quic/platform/api/quic_ptr_util.h"
#include "net/quic/platform/api/quic_str_cat.h"
#include "net/quic/platform/api/quic_text_utils.h"

using std::string;

namespace net {

size_t GetPacketHeaderSize(QuicTransportVersion version,
                           const QuicPacketHeader& header) {
  return GetPacketHeaderSize(version, header.connection_id_length,
                             header.version_flag, header.nonce != nullptr,
                             header.packet_number_length);
}

size_t GetPacketHeaderSize(QuicTransportVersion version,
                           QuicConnectionIdLength connection_id_length,
                           bool include_version,
                           bool include_diversification_nonce,
                           QuicPacketNumberLength packet_number_length) {
  return kPublicFlagsSize + connection_id_length +
         (include_version ? kQuicVersionSize : 0) + packet_number_length +
         (include_diversification_nonce ? kDiversificationNonceSize : 0);
}

size_t GetStartOfEncryptedData(QuicTransportVersion version,
                               const QuicPacketHeader& header) {
  return GetPacketHeaderSize(version, header);
}

size_t GetStartOfEncryptedData(QuicTransportVersion version,
                               QuicConnectionIdLength connection_id_length,
                               bool include_version,
                               bool include_diversification_nonce,
                               QuicPacketNumberLength packet_number_length) {
  // Encryption starts before private flags.
  return GetPacketHeaderSize(version, connection_id_length, include_version,
                             include_diversification_nonce,
                             packet_number_length);
}

QuicPacketHeader::QuicPacketHeader()
    : connection_id(0),
      connection_id_length(PACKET_8BYTE_CONNECTION_ID),
      reset_flag(false),
      version_flag(false),
      packet_number_length(PACKET_4BYTE_PACKET_NUMBER),
      version(
          ParsedQuicVersion(PROTOCOL_UNSUPPORTED, QUIC_VERSION_UNSUPPORTED)),
      nonce(nullptr),
      packet_number(0) {}

QuicPacketHeader::QuicPacketHeader(const QuicPacketHeader& other) = default;

QuicPacketHeader::~QuicPacketHeader() {}

QuicPublicResetPacket::QuicPublicResetPacket()
    : connection_id(0), nonce_proof(0) {}

QuicPublicResetPacket::QuicPublicResetPacket(QuicConnectionId connection_id)
    : connection_id(connection_id), nonce_proof(0) {}

QuicVersionNegotiationPacket::QuicVersionNegotiationPacket()
    : connection_id(0) {}

QuicVersionNegotiationPacket::QuicVersionNegotiationPacket(
    QuicConnectionId connection_id)
    : connection_id(connection_id) {}

QuicVersionNegotiationPacket::QuicVersionNegotiationPacket(
    const QuicVersionNegotiationPacket& other) = default;

QuicVersionNegotiationPacket::~QuicVersionNegotiationPacket() {}

QuicIetfStatelessResetPacket::QuicIetfStatelessResetPacket()
    : stateless_reset_token(0) {}

QuicIetfStatelessResetPacket::QuicIetfStatelessResetPacket(
    const QuicPacketHeader& header,
    uint128 token)
    : header(header), stateless_reset_token(token) {}

QuicIetfStatelessResetPacket::QuicIetfStatelessResetPacket(
    const QuicIetfStatelessResetPacket& other) = default;

QuicIetfStatelessResetPacket::~QuicIetfStatelessResetPacket() {}

std::ostream& operator<<(std::ostream& os, const QuicPacketHeader& header) {
  os << "{ connection_id: " << header.connection_id
     << ", connection_id_length: " << header.connection_id_length
     << ", packet_number_length: " << header.packet_number_length
     << ", reset_flag: " << header.reset_flag
     << ", version_flag: " << header.version_flag;
  if (header.version_flag) {
    os << ", version: " << ParsedQuicVersionToString(header.version);
  }
  if (header.nonce != nullptr) {
    os << ", diversification_nonce: "
       << QuicTextUtils::HexEncode(
              QuicStringPiece(header.nonce->data(), header.nonce->size()));
  }
  os << ", packet_number: " << header.packet_number << " }\n";
  return os;
}

QuicData::QuicData(const char* buffer, size_t length)
    : buffer_(buffer), length_(length), owns_buffer_(false) {}

QuicData::QuicData(const char* buffer, size_t length, bool owns_buffer)
    : buffer_(buffer), length_(length), owns_buffer_(owns_buffer) {}

QuicData::~QuicData() {
  if (owns_buffer_) {
    delete[] const_cast<char*>(buffer_);
  }
}

QuicPacket::QuicPacket(char* buffer,
                       size_t length,
                       bool owns_buffer,
                       QuicConnectionIdLength connection_id_length,
                       bool includes_version,
                       bool includes_diversification_nonce,
                       QuicPacketNumberLength packet_number_length)
    : QuicData(buffer, length, owns_buffer),
      buffer_(buffer),
      connection_id_length_(connection_id_length),
      includes_version_(includes_version),
      includes_diversification_nonce_(includes_diversification_nonce),
      packet_number_length_(packet_number_length) {}

QuicEncryptedPacket::QuicEncryptedPacket(const char* buffer, size_t length)
    : QuicData(buffer, length) {}

QuicEncryptedPacket::QuicEncryptedPacket(const char* buffer,
                                         size_t length,
                                         bool owns_buffer)
    : QuicData(buffer, length, owns_buffer) {}

std::unique_ptr<QuicEncryptedPacket> QuicEncryptedPacket::Clone() const {
  char* buffer = new char[this->length()];
  memcpy(buffer, this->data(), this->length());
  return QuicMakeUnique<QuicEncryptedPacket>(buffer, this->length(), true);
}

std::ostream& operator<<(std::ostream& os, const QuicEncryptedPacket& s) {
  os << s.length() << "-byte data";
  return os;
}

QuicReceivedPacket::QuicReceivedPacket(const char* buffer,
                                       size_t length,
                                       QuicTime receipt_time)
    : QuicEncryptedPacket(buffer, length),
      receipt_time_(receipt_time),
      ttl_(0) {}

QuicReceivedPacket::QuicReceivedPacket(const char* buffer,
                                       size_t length,
                                       QuicTime receipt_time,
                                       bool owns_buffer)
    : QuicEncryptedPacket(buffer, length, owns_buffer),
      receipt_time_(receipt_time),
      ttl_(0) {}

QuicReceivedPacket::QuicReceivedPacket(const char* buffer,
                                       size_t length,
                                       QuicTime receipt_time,
                                       bool owns_buffer,
                                       int ttl,
                                       bool ttl_valid)
    : QuicEncryptedPacket(buffer, length, owns_buffer),
      receipt_time_(receipt_time),
      ttl_(ttl_valid ? ttl : -1) {}

std::unique_ptr<QuicReceivedPacket> QuicReceivedPacket::Clone() const {
  char* buffer = new char[this->length()];
  memcpy(buffer, this->data(), this->length());
  return QuicMakeUnique<QuicReceivedPacket>(
      buffer, this->length(), receipt_time(), true, ttl(), ttl() >= 0);
}

std::ostream& operator<<(std::ostream& os, const QuicReceivedPacket& s) {
  os << s.length() << "-byte data";
  return os;
}

QuicStringPiece QuicPacket::AssociatedData(QuicTransportVersion version) const {
  return QuicStringPiece(
      data(), GetStartOfEncryptedData(
                  version, connection_id_length_, includes_version_,
                  includes_diversification_nonce_, packet_number_length_));
}

QuicStringPiece QuicPacket::Plaintext(QuicTransportVersion version) const {
  const size_t start_of_encrypted_data = GetStartOfEncryptedData(
      version, connection_id_length_, includes_version_,
      includes_diversification_nonce_, packet_number_length_);
  return QuicStringPiece(data() + start_of_encrypted_data,
                         length() - start_of_encrypted_data);
}

SerializedPacket::SerializedPacket(QuicPacketNumber packet_number,
                                   QuicPacketNumberLength packet_number_length,
                                   const char* encrypted_buffer,
                                   QuicPacketLength encrypted_length,
                                   bool has_ack,
                                   bool has_stop_waiting)
    : encrypted_buffer(encrypted_buffer),
      encrypted_length(encrypted_length),
      has_crypto_handshake(NOT_HANDSHAKE),
      num_padding_bytes(0),
      packet_number(packet_number),
      packet_number_length(packet_number_length),
      encryption_level(ENCRYPTION_NONE),
      has_ack(has_ack),
      has_stop_waiting(has_stop_waiting),
      transmission_type(NOT_RETRANSMISSION),
      original_packet_number(0),
      largest_acked(0) {}

SerializedPacket::SerializedPacket(const SerializedPacket& other) = default;

SerializedPacket& SerializedPacket::operator=(const SerializedPacket& other) =
    default;

SerializedPacket::SerializedPacket(SerializedPacket&& other)
    : encrypted_buffer(other.encrypted_buffer),
      encrypted_length(other.encrypted_length),
      has_crypto_handshake(other.has_crypto_handshake),
      num_padding_bytes(other.num_padding_bytes),
      packet_number(other.packet_number),
      packet_number_length(other.packet_number_length),
      encryption_level(other.encryption_level),
      has_ack(other.has_ack),
      has_stop_waiting(other.has_stop_waiting),
      transmission_type(other.transmission_type),
      original_packet_number(other.original_packet_number),
      largest_acked(other.largest_acked) {
  retransmittable_frames.swap(other.retransmittable_frames);
}

SerializedPacket::~SerializedPacket() {}

void ClearSerializedPacket(SerializedPacket* serialized_packet) {
  if (!serialized_packet->retransmittable_frames.empty()) {
    DeleteFrames(&serialized_packet->retransmittable_frames);
  }
  serialized_packet->encrypted_buffer = nullptr;
  serialized_packet->encrypted_length = 0;
  serialized_packet->largest_acked = 0;
}

char* CopyBuffer(const SerializedPacket& packet) {
  char* dst_buffer = new char[packet.encrypted_length];
  memcpy(dst_buffer, packet.encrypted_buffer, packet.encrypted_length);
  return dst_buffer;
}

}  // namespace net
