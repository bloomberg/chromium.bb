// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_hid_message.h"

#include <algorithm>
#include <numeric>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"

namespace device {

// static
std::unique_ptr<FidoHidMessage> FidoHidMessage::Create(
    uint32_t channel_id,
    CtapHidDeviceCommand type,
    base::span<const uint8_t> data) {
  if (data.size() > kHidMaxMessageSize)
    return nullptr;

  switch (type) {
    case CtapHidDeviceCommand::kCtapHidPing:
      break;
    case CtapHidDeviceCommand::kCtapHidMsg:
    case CtapHidDeviceCommand::kCtapHidCBOR: {
      if (data.empty())
        return nullptr;
      break;
    }

    case CtapHidDeviceCommand::kCtapHidCancel:
    case CtapHidDeviceCommand::kCtapHidWink: {
      if (!data.empty())
        return nullptr;
      break;
    }
    case CtapHidDeviceCommand::kCtapHidLock: {
      if (data.size() != 1 || data[0] > kHidMaxLockSeconds)
        return nullptr;
      break;
    }
    case CtapHidDeviceCommand::kCtapHidInit: {
      if (data.size() != 8)
        return nullptr;
      break;
    }
    case CtapHidDeviceCommand::kCtapHidKeepAlive:
    case CtapHidDeviceCommand::kCtapHidError:
      if (data.size() != 1)
        return nullptr;
  }

  return base::WrapUnique(new FidoHidMessage(channel_id, type, data));
}

// static
std::unique_ptr<FidoHidMessage> FidoHidMessage::CreateFromSerializedData(
    base::span<const uint8_t> serialized_data) {
  size_t remaining_size = 0;
  if (serialized_data.size() > kHidPacketSize ||
      serialized_data.size() < kHidInitPacketHeaderSize)
    return nullptr;

  auto init_packet = FidoHidInitPacket::CreateFromSerializedData(
      serialized_data, &remaining_size);

  if (init_packet == nullptr)
    return nullptr;

  return base::WrapUnique(
      new FidoHidMessage(std::move(init_packet), remaining_size));
}

FidoHidMessage::~FidoHidMessage() = default;

bool FidoHidMessage::MessageComplete() const {
  return remaining_size_ == 0;
}

std::vector<uint8_t> FidoHidMessage::GetMessagePayload() const {
  std::vector<uint8_t> data;
  size_t data_size = 0;
  for (const auto& packet : packets_) {
    data_size += packet->GetPacketPayload().size();
  }
  data.reserve(data_size);

  for (const auto& packet : packets_) {
    const auto& packet_data = packet->GetPacketPayload();
    data.insert(std::end(data), packet_data.cbegin(), packet_data.cend());
  }

  return data;
}

std::vector<uint8_t> FidoHidMessage::PopNextPacket() {
  if (packets_.empty())
    return {};

  std::vector<uint8_t> data = packets_.front()->GetSerializedData();
  packets_.pop_front();
  return data;
}

bool FidoHidMessage::AddContinuationPacket(base::span<const uint8_t> buf) {
  size_t remaining_size = remaining_size_;
  auto cont_packet =
      FidoHidContinuationPacket::CreateFromSerializedData(buf, &remaining_size);

  // Reject packets with a different channel id.
  if (!cont_packet || channel_id_ != cont_packet->channel_id())
    return false;

  remaining_size_ = remaining_size;
  packets_.push_back(std::move(cont_packet));
  return true;
}

size_t FidoHidMessage::NumPackets() const {
  return packets_.size();
}

FidoHidMessage::FidoHidMessage(uint32_t channel_id,
                               CtapHidDeviceCommand type,
                               base::span<const uint8_t> data)
    : channel_id_(channel_id) {
  uint8_t sequence = 0;

  auto init_data = data.first(std::min(kHidInitPacketDataSize, data.size()));
  packets_.push_back(std::make_unique<FidoHidInitPacket>(
      channel_id, type,
      std::vector<uint8_t>(init_data.begin(), init_data.end()), data.size()));
  data = data.subspan(init_data.size());

  while (!data.empty()) {
    auto cont_data =
        data.first(std::min(kHidContinuationPacketDataSize, data.size()));
    packets_.push_back(std::make_unique<FidoHidContinuationPacket>(
        channel_id, sequence++,
        std::vector<uint8_t>(cont_data.begin(), cont_data.end())));
    data = data.subspan(cont_data.size());
  }
}

FidoHidMessage::FidoHidMessage(std::unique_ptr<FidoHidInitPacket> init_packet,
                               size_t remaining_size)
    : remaining_size_(remaining_size) {
  channel_id_ = init_packet->channel_id();
  cmd_ = init_packet->command();
  packets_.push_back(std::move(init_packet));
}

}  // namespace device
