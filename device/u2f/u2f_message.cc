// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "device/u2f/u2f_packet.h"

#include "u2f_message.h"

namespace device {

// static
std::unique_ptr<U2fMessage> U2fMessage::Create(
    uint32_t channel_id,
    U2fCommandType type,
    const std::vector<uint8_t>& data) {
  if (data.size() > kMaxMessageSize)
    return nullptr;

  return std::make_unique<U2fMessage>(channel_id, type, data);
}

// static
std::unique_ptr<U2fMessage> U2fMessage::CreateFromSerializedData(
    const std::vector<uint8_t>& buf) {
  size_t remaining_size = 0;
  if (buf.size() > U2fPacket::kPacketSize || buf.size() < kInitPacketHeader)
    return nullptr;

  std::unique_ptr<U2fInitPacket> init_packet =
      U2fInitPacket::CreateFromSerializedData(buf, &remaining_size);
  if (init_packet == nullptr)
    return nullptr;

  return std::make_unique<U2fMessage>(std::move(init_packet), remaining_size);
}

U2fMessage::U2fMessage(std::unique_ptr<U2fInitPacket> init_packet,
                       size_t remaining_size)
    : remaining_size_(remaining_size) {
  channel_id_ = init_packet->channel_id();
  packets_.push_back(std::move(init_packet));
}

U2fMessage::U2fMessage(uint32_t channel_id,
                       U2fCommandType type,
                       const std::vector<uint8_t>& data)
    : packets_(), remaining_size_(), channel_id_(channel_id) {
  size_t remaining_bytes = data.size();
  uint8_t sequence = 0;

  std::vector<uint8_t>::const_iterator first = data.begin();
  std::vector<uint8_t>::const_iterator last;

  if (remaining_bytes > kInitPacketDataSize) {
    last = data.begin() + kInitPacketDataSize;
    remaining_bytes -= kInitPacketDataSize;
  } else {
    last = data.begin() + remaining_bytes;
    remaining_bytes = 0;
  }

  packets_.push_back(std::make_unique<U2fInitPacket>(
      channel_id, static_cast<uint8_t>(type), std::vector<uint8_t>(first, last),
      data.size()));

  while (remaining_bytes > 0) {
    first = last;
    if (remaining_bytes > kContinuationPacketDataSize) {
      last = first + kContinuationPacketDataSize;
      remaining_bytes -= kContinuationPacketDataSize;
    } else {
      last = first + remaining_bytes;
      remaining_bytes = 0;
    }

    packets_.push_back(std::make_unique<U2fContinuationPacket>(
        channel_id, sequence, std::vector<uint8_t>(first, last)));
    sequence++;
  }
}

U2fMessage::~U2fMessage() = default;

std::list<std::unique_ptr<U2fPacket>>::const_iterator U2fMessage::begin() {
  return packets_.cbegin();
}

std::list<std::unique_ptr<U2fPacket>>::const_iterator U2fMessage::end() {
  return packets_.cend();
}

std::vector<uint8_t> U2fMessage::PopNextPacket() {
  std::vector<uint8_t> data;
  if (NumPackets() > 0) {
    data = packets_.front()->GetSerializedData();
    packets_.pop_front();
  }
  return data;
}

bool U2fMessage::AddContinuationPacket(const std::vector<uint8_t>& buf) {
  size_t remaining_size = remaining_size_;
  std::unique_ptr<U2fContinuationPacket> cont_packet =
      U2fContinuationPacket::CreateFromSerializedData(buf, &remaining_size);

  // Reject packets with a different channel id
  if (cont_packet == nullptr || channel_id_ != cont_packet->channel_id())
    return false;

  remaining_size_ = remaining_size;
  packets_.push_back(std::move(cont_packet));
  return true;
}

bool U2fMessage::MessageComplete() {
  return remaining_size_ == 0;
}

std::vector<uint8_t> U2fMessage::GetMessagePayload() const {
  std::vector<uint8_t> data;

  for (const auto& packet : packets_) {
    std::vector<uint8_t> packet_data = packet->GetPacketPayload();
    data.insert(std::end(data), packet_data.cbegin(), packet_data.cend());
  }

  return data;
}

size_t U2fMessage::NumPackets() {
  return packets_.size();
}

}  // namespace device
