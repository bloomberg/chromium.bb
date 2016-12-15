// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_packet.h"
#include "net/base/io_buffer.h"

namespace device {

U2fPacket::U2fPacket(const std::vector<uint8_t> data, const uint32_t channel_id)
    : data_(data), channel_id_(channel_id) {}

U2fPacket::U2fPacket() {}

U2fPacket::~U2fPacket() {}

scoped_refptr<net::IOBufferWithSize> U2fPacket::GetSerializedBuffer() {
  if (serialized_)
    return serialized_;
  else
    return make_scoped_refptr(new net::IOBufferWithSize(0));
}

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
U2fInitPacket::U2fInitPacket(const uint32_t channel_id,
                             const uint8_t cmd,
                             const std::vector<uint8_t> data,
                             const uint16_t payload_length)
    : U2fPacket(data, channel_id), command_(cmd) {
  serialized_ = new net::IOBufferWithSize(kPacketSize);
  size_t index = 0;
  // Byte at offset 0 is the report ID, which is always 0
  serialized_->data()[index++] = 0;

  serialized_->data()[index++] = (channel_id_ >> 24) & 0xff;
  serialized_->data()[index++] = (channel_id_ >> 16) & 0xff;
  serialized_->data()[index++] = (channel_id_ >> 8) & 0xff;
  serialized_->data()[index++] = channel_id_ & 0xff;

  serialized_->data()[index++] = command_;
  payload_length_ = payload_length;
  serialized_->data()[index++] = (payload_length >> 8) & 0xff;
  serialized_->data()[index++] = payload_length & 0xff;
  for (size_t data_idx = 0; data_idx < data_.size(); ++data_idx)
    serialized_->data()[index++] = data_.at(data_idx);
  while (static_cast<int>(index) < serialized_->size())
    serialized_->data()[index++] = 0;
}

// static
scoped_refptr<U2fInitPacket> U2fInitPacket::CreateFromSerializedData(
    scoped_refptr<net::IOBufferWithSize> buf,
    size_t* remaining_size) {
  if (buf == nullptr || remaining_size == nullptr || buf->size() != kPacketSize)
    return nullptr;

  return make_scoped_refptr(new U2fInitPacket(buf, remaining_size));
}

U2fInitPacket::U2fInitPacket(scoped_refptr<net::IOBufferWithSize> buf,
                             size_t* remaining_size) {
  // Report ID is at index 0, so start at index 1 for channel ID
  size_t index = 1;
  uint16_t payload_size = 0;
  uint16_t data_size = 0;

  channel_id_ = (buf->data()[index++] & 0xff) << 24;
  channel_id_ |= (buf->data()[index++] & 0xff) << 16;
  channel_id_ |= (buf->data()[index++] & 0xff) << 8;
  channel_id_ |= buf->data()[index++] & 0xff;
  command_ = buf->data()[index++];
  payload_size = buf->data()[index++] << 8;
  payload_size |= static_cast<uint8_t>(buf->data()[index++]);
  payload_length_ = payload_size;

  // Check to see if payload is less than maximum size and padded with 0s
  data_size =
      std::min(payload_size, static_cast<uint16_t>(kPacketSize - index));
  // Update remaining size to determine the payload size of follow on packets
  *remaining_size = payload_size - data_size;

  data_.insert(data_.end(), &buf->data()[index],
               &buf->data()[index + data_size]);

  for (int i = index + data_size; i < buf->size(); ++i)
    buf->data()[i] = 0;
  serialized_ = buf;
}

U2fInitPacket::~U2fInitPacket() {}

// U2F Continuation packet is defined as:
// Offset Length
// 0      4       Channel ID
// 4      1       Packet sequence 0x00..0x7f
// 5      (s-5)   Payload data
U2fContinuationPacket::U2fContinuationPacket(const uint32_t channel_id,
                                             const uint8_t sequence,
                                             std::vector<uint8_t> data)
    : U2fPacket(data, channel_id), sequence_(sequence) {
  serialized_ = new net::IOBufferWithSize(kPacketSize);
  size_t index = 0;
  // Byte at offset 0 is the report ID, which is always 0
  serialized_->data()[index++] = 0;

  serialized_->data()[index++] = (channel_id_ >> 24) & 0xff;
  serialized_->data()[index++] = (channel_id_ >> 16) & 0xff;
  serialized_->data()[index++] = (channel_id_ >> 8) & 0xff;
  serialized_->data()[index++] = channel_id_ & 0xff;

  serialized_->data()[index++] = sequence_;
  for (size_t idx = 0; idx < data_.size(); ++idx)
    serialized_->data()[index++] = data_.at(idx);

  while (static_cast<int>(index) < serialized_->size())
    serialized_->data()[index++] = 0;
}

// static
scoped_refptr<U2fContinuationPacket>
U2fContinuationPacket::CreateFromSerializedData(
    scoped_refptr<net::IOBufferWithSize> buf,
    size_t* remaining_size) {
  if (buf == nullptr || remaining_size == nullptr || buf->size() != kPacketSize)
    return nullptr;

  return make_scoped_refptr(new U2fContinuationPacket(buf, remaining_size));
}

U2fContinuationPacket::U2fContinuationPacket(
    scoped_refptr<net::IOBufferWithSize> buf,
    size_t* remaining_size) {
  // Report ID is at index 0, so start at index 1 for channel ID
  size_t index = 1;
  size_t data_size;

  channel_id_ = (buf->data()[index++] & 0xff) << 24;
  channel_id_ |= (buf->data()[index++] & 0xff) << 16;
  channel_id_ |= (buf->data()[index++] & 0xff) << 8;
  channel_id_ |= buf->data()[index++] & 0xff;
  sequence_ = buf->data()[index++];

  // Check to see if packet payload is less than maximum size and padded with 0s
  data_size = std::min(*remaining_size, kPacketSize - index);
  *remaining_size -= data_size;
  data_.insert(std::end(data_), &buf->data()[index],
               &buf->data()[index + data_size]);

  // Incoming buffer may not be padded with 0's, so manually update buffer
  for (int i = index + data_size; i < buf->size(); ++i)
    buf->data()[i] = 0;
  serialized_ = buf;
}

U2fContinuationPacket::~U2fContinuationPacket() {}
}  // namespace device
