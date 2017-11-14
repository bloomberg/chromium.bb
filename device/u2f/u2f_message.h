// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_MESSAGE_H_
#define DEVICE_U2F_U2F_MESSAGE_H_

#include <list>
#include <vector>

#include "base/gtest_prod_util.h"
#include "device/u2f/u2f_command_type.h"
#include "device/u2f/u2f_packet.h"

namespace device {

// U2fMessages are defined by the specification at
// http://fidoalliance.org/specs/u2f-specs-1.0-bt-nfc-id-amendment/fido-u2f-hid-protocol.html
// A U2fMessage object represents a list of U2fPackets. Basic information
// about the message are available through class methods.
class U2fMessage {
 public:
  U2fMessage(uint32_t channel_id,
             U2fCommandType type,
             const std::vector<uint8_t>& data);
  U2fMessage(std::unique_ptr<U2fInitPacket> init_packet, size_t remaining_size);
  ~U2fMessage();

  static std::unique_ptr<U2fMessage> Create(uint32_t channel_id,
                                            U2fCommandType type,
                                            const std::vector<uint8_t>& data);
  // Reconstruct a message from serialized message data
  static std::unique_ptr<U2fMessage> CreateFromSerializedData(
      const std::vector<uint8_t>& buf);
  // Pop front of queue with next packet
  std::vector<uint8_t> PopNextPacket();
  // Adds a continuation packet to the packet list, from the serialized
  // response value
  bool AddContinuationPacket(const std::vector<uint8_t>& packet_buf);
  size_t NumPackets();
  // Returns entire message payload
  std::vector<uint8_t> GetMessagePayload() const;
  uint32_t channel_id() { return channel_id_; }
  // Message construction complete
  bool MessageComplete();
  std::list<std::unique_ptr<U2fPacket>>::const_iterator begin();
  std::list<std::unique_ptr<U2fPacket>>::const_iterator end();

 private:
  FRIEND_TEST_ALL_PREFIXES(U2fMessageTest, TestMaxLengthPacketConstructors);
  FRIEND_TEST_ALL_PREFIXES(U2fMessageTest, TestMessagePartitoning);
  FRIEND_TEST_ALL_PREFIXES(U2fMessageTest, TestDeconstruct);
  FRIEND_TEST_ALL_PREFIXES(U2fMessageTest, TestMaxSize);
  FRIEND_TEST_ALL_PREFIXES(U2fMessageTest, TestDeserialize);

  static constexpr size_t kInitPacketHeader = 7;
  static constexpr size_t kContinuationPacketHeader = 5;
  static constexpr size_t kMaxHidPacketSize = 64;
  static constexpr size_t kInitPacketDataSize =
      kMaxHidPacketSize - kInitPacketHeader;
  static constexpr size_t kContinuationPacketDataSize =
      kMaxHidPacketSize - kContinuationPacketHeader;
  // Messages are limited to an init packet and 128 continuation packets
  // Maximum payload length therefore is 64-7 + 128 * (64-5) = 7609 bytes
  static constexpr size_t kMaxMessageSize = 7609;

  std::list<std::unique_ptr<U2fPacket>> packets_;
  size_t remaining_size_;
  uint32_t channel_id_;
};
}  // namespace device

#endif  // DEVICE_U2F_U2F_MESSAGE_H_
