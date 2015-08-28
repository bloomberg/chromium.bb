// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_fec_group.h"

#include <limits>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/stl_util.h"

using base::StringPiece;
using std::numeric_limits;
using std::set;

namespace net {

QuicFecGroup::QuicFecGroup()
    : min_protected_packet_(kInvalidPacketNumber),
      max_protected_packet_(kInvalidPacketNumber),
      payload_parity_len_(0),
      effective_encryption_level_(NUM_ENCRYPTION_LEVELS) {}

QuicFecGroup::~QuicFecGroup() {}

bool QuicFecGroup::Update(EncryptionLevel encryption_level,
                          const QuicPacketHeader& header,
                          StringPiece decrypted_payload) {
  DCHECK_NE(kInvalidPacketNumber, header.packet_packet_number);
  if (ContainsKey(received_packets_, header.packet_packet_number)) {
    return false;
  }
  if (min_protected_packet_ != kInvalidPacketNumber &&
      max_protected_packet_ != kInvalidPacketNumber &&
      (header.packet_packet_number < min_protected_packet_ ||
       header.packet_packet_number > max_protected_packet_)) {
    DLOG(ERROR) << "FEC group does not cover received packet: "
                << header.packet_packet_number;
    return false;
  }
  if (!UpdateParity(decrypted_payload)) {
    return false;
  }
  received_packets_.insert(header.packet_packet_number);
  if (encryption_level < effective_encryption_level_) {
    effective_encryption_level_ = encryption_level;
  }
  return true;
}

bool QuicFecGroup::UpdateFec(EncryptionLevel encryption_level,
                             QuicPacketNumber fec_packet_packet_number,
                             const QuicFecData& fec) {
  DCHECK_NE(kInvalidPacketNumber, fec_packet_packet_number);
  DCHECK_NE(kInvalidPacketNumber, fec.fec_group);
  if (min_protected_packet_ != kInvalidPacketNumber) {
    return false;
  }
  PacketNumberSet::const_iterator it = received_packets_.begin();
  while (it != received_packets_.end()) {
    if ((*it < fec.fec_group) || (*it >= fec_packet_packet_number)) {
      DLOG(ERROR) << "FEC group does not cover received packet: " << *it;
      return false;
    }
    ++it;
  }
  if (!UpdateParity(fec.redundancy)) {
    return false;
  }
  min_protected_packet_ = fec.fec_group;
  max_protected_packet_ = fec_packet_packet_number - 1;
  if (encryption_level < effective_encryption_level_) {
    effective_encryption_level_ = encryption_level;
  }
  return true;
}

bool QuicFecGroup::CanRevive() const {
  // We can revive if we're missing exactly 1 packet.
  return NumMissingPackets() == 1;
}

bool QuicFecGroup::IsFinished() const {
  // We are finished if we are not missing any packets.
  return NumMissingPackets() == 0;
}

size_t QuicFecGroup::Revive(QuicPacketHeader* header,
                            char* decrypted_payload,
                            size_t decrypted_payload_len) {
  if (!CanRevive()) {
    return 0;
  }

  // Identify the packet number to be resurrected.
  QuicPacketNumber missing = kInvalidPacketNumber;
  for (QuicPacketNumber i = min_protected_packet_; i <= max_protected_packet_;
       ++i) {
    // Is this packet missing?
    if (received_packets_.count(i) == 0) {
      missing = i;
      break;
    }
  }
  DCHECK_NE(kInvalidPacketNumber, missing);

  DCHECK_LE(payload_parity_len_, decrypted_payload_len);
  if (payload_parity_len_ > decrypted_payload_len) {
    return 0;
  }
  for (size_t i = 0; i < payload_parity_len_; ++i) {
    decrypted_payload[i] = payload_parity_[i];
  }

  header->packet_packet_number = missing;
  header->entropy_flag = false;  // Unknown entropy.

  received_packets_.insert(missing);
  return payload_parity_len_;
}

bool QuicFecGroup::ProtectsPacketsBefore(QuicPacketNumber num) const {
  if (max_protected_packet_ != kInvalidPacketNumber) {
    return max_protected_packet_ < num;
  }
  // Since we might not yet have received the FEC packet, we must check
  // the packets we have received.
  return *received_packets_.begin() < num;
}

bool QuicFecGroup::UpdateParity(StringPiece payload) {
  DCHECK_GE(kMaxPacketSize, payload.size());
  if (payload.size() > kMaxPacketSize) {
    DLOG(ERROR) << "Illegal payload size: " << payload.size();
    return false;
  }
  if (payload_parity_len_ < payload.size()) {
    payload_parity_len_ = payload.size();
  }
  if (received_packets_.empty() &&
      min_protected_packet_ == kInvalidPacketNumber) {
    // Initialize the parity to the value of this payload
    memcpy(payload_parity_, payload.data(), payload.size());
    if (payload.size() < kMaxPacketSize) {
      // TODO(rch): expand as needed.
      memset(payload_parity_ + payload.size(), 0,
             kMaxPacketSize - payload.size());
    }
    return true;
  }
  // Update the parity by XORing in the data (padding with 0s if necessary).
  XorBuffers(payload.data(), payload.size(), payload_parity_);
  return true;
}

QuicPacketCount QuicFecGroup::NumMissingPackets() const {
  if (min_protected_packet_ == kInvalidPacketNumber) {
    return numeric_limits<QuicPacketCount>::max();
  }
  return static_cast<QuicPacketCount>(
      (max_protected_packet_ - min_protected_packet_ + 1) -
      received_packets_.size());
}

void QuicFecGroup::XorBuffers(const char* input,
                              size_t size_in_bytes,
                              char* output) {
#if defined(__i386__) || defined(__x86_64__)
  // On x86, alignment is not required and casting bytes to words is safe.

  // size_t is a reasonable approximation of how large a general-purpose
  // register is for the platforms and compilers Chrome is built on.
  typedef size_t platform_word;
  const size_t size_in_words = size_in_bytes / sizeof(platform_word);

  const platform_word* input_words =
      reinterpret_cast<const platform_word*>(input);
  platform_word* output_words = reinterpret_cast<platform_word*>(output);

  // Handle word-sized part of the buffer.
  size_t offset_in_words = 0;
  for (; offset_in_words < size_in_words; offset_in_words++) {
    output_words[offset_in_words] ^= input_words[offset_in_words];
  }

  // Handle the tail which does not fit into the word.
  for (size_t offset_in_bytes = offset_in_words * sizeof(platform_word);
       offset_in_bytes < size_in_bytes; offset_in_bytes++) {
    output[offset_in_bytes] ^= input[offset_in_bytes];
  }
#else
  // On ARM and most other plaforms, the code above could fail due to the
  // alignment errors.  Stick to byte-by-byte comparison.
  for (size_t offset = 0; offset < size_in_bytes; offset++) {
    output[offset] ^= input[offset];
  }
#endif /* defined(__i386__) || defined(__x86_64__) */
}

}  // namespace net
