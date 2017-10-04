// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "base/memory/ptr_util.h"
#include "net/base/io_buffer.h"

#include "u2f_packet.h"

namespace device {

U2fPacket::U2fPacket(const std::vector<uint8_t>& data, uint32_t channel_id)
    : data_(data), channel_id_(channel_id) {}

U2fPacket::U2fPacket() {}

U2fPacket::~U2fPacket() {}

std::vector<uint8_t> U2fPacket::GetPacketPayload() const {
  return data_;
}

// U2F Initialization packet is defined as:
// Offset Length
// 0      4       Channel ID
// 4      1       Command ID
// 5      1       High order packet payload size
// 6      1       Low order packet payload size
// 7      (s-7)   Payload data
U2fInitPacket::U2fInitPacket(uint32_t channel_id,
                             uint8_t cmd,
                             const std::vector<uint8_t>& data,
                             uint16_t payload_length)
    : U2fPacket(data, channel_id),
      command_(cmd),
      payload_length_(payload_length) {}

scoped_refptr<net::IOBufferWithSize> U2fInitPacket::GetSerializedData() {
  auto serialized =
      base::WrapRefCounted(new net::IOBufferWithSize(kPacketSize));
  size_t index = 0;
  // Byte at offset 0 is the report ID, which is always 0
  serialized->data()[index++] = 0;
  serialized->data()[index++] = (channel_id_ >> 24) & 0xff;
  serialized->data()[index++] = (channel_id_ >> 16) & 0xff;
  serialized->data()[index++] = (channel_id_ >> 8) & 0xff;
  serialized->data()[index++] = channel_id_ & 0xff;

  serialized->data()[index++] = command_;
  serialized->data()[index++] = (payload_length_ >> 8) & 0xff;
  serialized->data()[index++] = payload_length_ & 0xff;
  std::memcpy(&serialized->data()[index], data_.data(), data_.size());
  index += data_.size();

  std::memset(&serialized->data()[index], 0, serialized->size() - index);
  return serialized;
}

// static
std::unique_ptr<U2fInitPacket> U2fInitPacket::CreateFromSerializedData(
    const std::vector<uint8_t>& serialized,
    size_t* remaining_size) {
  if (remaining_size == nullptr || serialized.size() != kPacketSize)
    return nullptr;

  return std::make_unique<U2fInitPacket>(serialized, remaining_size);
}

U2fInitPacket::U2fInitPacket(const std::vector<uint8_t>& serialized,
                             size_t* remaining_size) {
  // Report ID is at index 0, so start at index 1 for channel ID
  size_t index = 1;
  uint16_t payload_size = 0;

  channel_id_ = (serialized[index++] & 0xff) << 24;
  channel_id_ |= (serialized[index++] & 0xff) << 16;
  channel_id_ |= (serialized[index++] & 0xff) << 8;
  channel_id_ |= serialized[index++] & 0xff;
  command_ = serialized[index++];
  payload_size = serialized[index++] << 8;
  payload_size |= serialized[index++];
  payload_length_ = payload_size;

  // Check to see if payload is less than maximum size and padded with 0s
  uint16_t data_size =
      std::min(payload_size, static_cast<uint16_t>(kPacketSize - index));
  // Update remaining size to determine the payload size of follow on packets
  *remaining_size = payload_size - data_size;

  data_.insert(data_.end(), serialized.begin() + index,
               serialized.begin() + index + data_size);
}

U2fInitPacket::~U2fInitPacket() {}

// U2F Continuation packet is defined as:
// Offset Length
// 0      4       Channel ID
// 4      1       Packet sequence 0x00..0x7f
// 5      (s-5)   Payload data
U2fContinuationPacket::U2fContinuationPacket(const uint32_t channel_id,
                                             const uint8_t sequence,
                                             const std::vector<uint8_t>& data)
    : U2fPacket(data, channel_id), sequence_(sequence) {}

scoped_refptr<net::IOBufferWithSize>
U2fContinuationPacket::GetSerializedData() {
  auto serialized =
      base::WrapRefCounted(new net::IOBufferWithSize(kPacketSize));
  size_t index = 0;
  // Byte at offset 0 is the report ID, which is always 0
  serialized->data()[index++] = 0;
  serialized->data()[index++] = (channel_id_ >> 24) & 0xff;
  serialized->data()[index++] = (channel_id_ >> 16) & 0xff;
  serialized->data()[index++] = (channel_id_ >> 8) & 0xff;
  serialized->data()[index++] = channel_id_ & 0xff;

  serialized->data()[index++] = sequence_;
  std::memcpy(&serialized->data()[index], data_.data(), data_.size());
  index += data_.size();

  std::memset(&serialized->data()[index], 0, serialized->size() - index);
  return serialized;
}

// static
std::unique_ptr<U2fContinuationPacket>
U2fContinuationPacket::CreateFromSerializedData(
    const std::vector<uint8_t>& serialized,
    size_t* remaining_size) {
  if (remaining_size == nullptr || serialized.size() != kPacketSize)
    return nullptr;

  return std::make_unique<U2fContinuationPacket>(serialized, remaining_size);
}

U2fContinuationPacket::U2fContinuationPacket(
    const std::vector<uint8_t>& serialized,
    size_t* remaining_size) {
  // Report ID is at index 0, so start at index 1 for channel ID
  size_t index = 1;
  size_t data_size;

  channel_id_ = (serialized[index++] & 0xff) << 24;
  channel_id_ |= (serialized[index++] & 0xff) << 16;
  channel_id_ |= (serialized[index++] & 0xff) << 8;
  channel_id_ |= serialized[index++] & 0xff;
  sequence_ = serialized[index++];

  // Check to see if packet payload is less than maximum size and padded with 0s
  data_size = std::min(*remaining_size, kPacketSize - index);
  *remaining_size -= data_size;
  data_.insert(data_.end(), serialized.begin() + index,
               serialized.begin() + index + data_size);
}

U2fContinuationPacket::~U2fContinuationPacket() {}

}  // namespace device
