// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_PACKET_H_
#define DEVICE_U2F_U2F_PACKET_H_

#include <algorithm>
#include <vector>

#include "base/memory/ref_counted.h"

namespace device {

// U2fPackets are defined by the specification at
// http://fidoalliance.org/specs/u2f-specs-1.0-bt-nfc-id-amendment/fido-u2f-hid-protocol.html
// Packets are one of two types, initialization packets and continuation
// packets,
// (U2fInitPacket and U2fContinuationPacket). U2fPackets have header information
// and a payload. If a U2fInitPacket cannot store the entire payload, further
// payload information is stored in U2fContinuationPackets.
class U2fPacket {
 public:
  U2fPacket(const std::vector<uint8_t>& data, uint32_t channel_id);
  virtual ~U2fPacket();

  virtual std::vector<uint8_t> GetSerializedData() = 0;
  std::vector<uint8_t> GetPacketPayload() const;
  uint32_t channel_id() { return channel_id_; }

 protected:
  U2fPacket();

  static constexpr size_t kPacketSize = 64;
  std::vector<uint8_t> data_;
  uint32_t channel_id_;

 private:
  friend class U2fMessage;
};

// U2fInitPacket, based on the U2f specification consists of a header with data
// that is serialized into a IOBuffer. A channel identifier is allocated by the
// U2F device to ensure its system-wide uniqueness. Command identifiers
// determine the type of message the packet corresponds to. Payload length
// is the length of the entire message payload, and the data is only the portion
// of the payload that will fit into the U2fInitPacket.
class U2fInitPacket : public U2fPacket {
 public:
  U2fInitPacket(uint32_t channel_id,
                uint8_t cmd,
                const std::vector<uint8_t>& data,
                uint16_t payload_length);
  U2fInitPacket(const std::vector<uint8_t>& serialized, size_t* remaining_size);
  ~U2fInitPacket() final;

  // Creates a packet from the serialized data of an initialization packet. As
  // this is the first packet, the payload length of the entire message will be
  // included within the serialized data. Remaining size will be returned to
  // inform the callee how many additional packets to expect.
  static std::unique_ptr<U2fInitPacket> CreateFromSerializedData(
      const std::vector<uint8_t>& serialized,
      size_t* remaining_size);
  std::vector<uint8_t> GetSerializedData() final;
  uint8_t command() { return command_; }
  uint16_t payload_length() { return payload_length_; }

 private:
  uint8_t command_;
  uint16_t payload_length_;
};

// U2fContinuationPacket, based on the U2f Specification consists of a header
// with data that is serialized into an IOBuffer. The channel identifier will
// be identical to the identifier in all other packets of the message. The
// packet sequence will be the sequence number of this particular packet, from
// 0x00 to 0x7f.
class U2fContinuationPacket : public U2fPacket {
 public:
  U2fContinuationPacket(uint32_t channel_id,
                        uint8_t sequence,
                        const std::vector<uint8_t>& data);
  U2fContinuationPacket(const std::vector<uint8_t>& serialized,
                        size_t* remaining_size);
  ~U2fContinuationPacket() final;

  // Creates a packet from the serialized data of a continuation packet. As an
  // U2fInitPacket would have arrived earlier with the total payload size,
  // the remaining size should be passed to inform the packet of how much data
  // to expect.
  static std::unique_ptr<U2fContinuationPacket> CreateFromSerializedData(
      const std::vector<uint8_t>& serialized,
      size_t* remaining_size);
  std::vector<uint8_t> GetSerializedData() final;
  uint8_t sequence() { return sequence_; }

 private:
  uint8_t sequence_;
};
}  // namespace device

#endif  // DEVICE_U2F_U2F_PACKET_H_
