// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/u2f/u2f_message.h"
#include "net/base/io_buffer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

class U2fMessageTest : public testing::Test {};

// Packets should be 64 bytes + 1 report ID byte
TEST_F(U2fMessageTest, TestPacketSize) {
  uint32_t channel_id = 0x05060708;
  std::vector<uint8_t> data;

  scoped_refptr<U2fPacket> init_packet(
      new U2fInitPacket(channel_id, 0, data, data.size()));
  EXPECT_EQ(65, init_packet->GetSerializedBuffer()->size());

  scoped_refptr<U2fPacket> continuation_packet(
      new U2fContinuationPacket(channel_id, 0, data));
  EXPECT_EQ(65, continuation_packet->GetSerializedBuffer()->size());
}

/*
 * U2f Init Packets are of the format:
 * Byte 0:    0
 * Byte 1-4:  Channel ID
 * Byte 5:    Command byte
 * Byte 6-7:  Big Endian size of data
 * Byte 8-n:  Data block
 *
 * Remaining buffer is padded with 0
 */
TEST_F(U2fMessageTest, TestPacketData) {
  uint32_t channel_id = 0xF5060708;
  std::vector<uint8_t> data{10, 11};
  uint8_t cmd = static_cast<uint8_t>(U2fMessage::Type::CMD_WINK);
  scoped_refptr<U2fPacket> init_packet(
      new U2fInitPacket(channel_id, cmd, data, data.size()));
  int index = 0;

  EXPECT_EQ(0, init_packet->GetSerializedBuffer()->data()[index++]);
  EXPECT_EQ((channel_id >> 24) & 0xff,
            static_cast<uint8_t>(
                init_packet->GetSerializedBuffer()->data()[index++]));
  EXPECT_EQ((channel_id >> 16) & 0xff,
            static_cast<uint8_t>(
                init_packet->GetSerializedBuffer()->data()[index++]));
  EXPECT_EQ((channel_id >> 8) & 0xff,
            static_cast<uint8_t>(
                init_packet->GetSerializedBuffer()->data()[index++]));
  EXPECT_EQ(channel_id & 0xff,
            static_cast<uint8_t>(
                init_packet->GetSerializedBuffer()->data()[index++]));
  EXPECT_EQ(cmd, static_cast<uint8_t>(
                     init_packet->GetSerializedBuffer()->data()[index++]));

  EXPECT_EQ(data.size() >> 8,
            static_cast<uint8_t>(
                init_packet->GetSerializedBuffer()->data()[index++]));
  EXPECT_EQ(data.size() & 0xff,
            static_cast<uint8_t>(
                init_packet->GetSerializedBuffer()->data()[index++]));
  EXPECT_EQ(data.at(0), init_packet->GetSerializedBuffer()->data()[index++]);
  EXPECT_EQ(data.at(1), init_packet->GetSerializedBuffer()->data()[index++]);
  for (; index < init_packet->GetSerializedBuffer()->size(); index++) {
    EXPECT_EQ(0, init_packet->GetSerializedBuffer()->data()[index])
        << "mismatch at index " << index;
  }
}

TEST_F(U2fMessageTest, TestPacketConstructors) {
  uint32_t channel_id = 0x05060708;
  std::vector<uint8_t> data{10, 11};
  uint8_t cmd = static_cast<uint8_t>(U2fMessage::Type::CMD_WINK);
  scoped_refptr<U2fInitPacket> orig_packet(
      new U2fInitPacket(channel_id, cmd, data, data.size()));

  size_t payload_length = static_cast<size_t>(orig_packet->payload_length());
  scoped_refptr<U2fInitPacket> reconstructed_packet =
      U2fInitPacket::CreateFromSerializedData(
          orig_packet->GetSerializedBuffer(), &payload_length);
  EXPECT_EQ(orig_packet->command(), reconstructed_packet->command());
  EXPECT_EQ(orig_packet->payload_length(),
            reconstructed_packet->payload_length());
  EXPECT_THAT(orig_packet->GetPacketPayload(),
              testing::ContainerEq(reconstructed_packet->GetPacketPayload()));

  EXPECT_EQ(channel_id, reconstructed_packet->channel_id());

  ASSERT_EQ(orig_packet->GetSerializedBuffer()->size(),
            reconstructed_packet->GetSerializedBuffer()->size());
  for (size_t i = 0;
       i < static_cast<size_t>(orig_packet->GetSerializedBuffer()->size());
       ++i) {
    EXPECT_EQ(orig_packet->GetSerializedBuffer()->data()[i],
              reconstructed_packet->GetSerializedBuffer()->data()[i]);
  }
}

TEST_F(U2fMessageTest, TestMaxLengthPacketConstructors) {
  uint32_t channel_id = 0xAAABACAD;
  std::vector<uint8_t> data;
  for (size_t i = 0; i < U2fMessage::kMaxMessageSize; ++i)
    data.push_back(static_cast<uint8_t>(i % 0xff));

  U2fMessage::Type cmd = U2fMessage::Type::CMD_MSG;
  scoped_refptr<U2fMessage> orig_msg =
      U2fMessage::Create(channel_id, cmd, data);
  auto it = orig_msg->begin();
  scoped_refptr<U2fMessage> new_msg =
      U2fMessage::CreateFromSerializedData((*it)->GetSerializedBuffer());
  it++;

  for (; it != orig_msg->end(); ++it)
    new_msg->AddContinuationPacket((*it)->GetSerializedBuffer());

  auto orig_it = orig_msg->begin();
  auto new_it = new_msg->begin();

  for (; orig_it != orig_msg->end() && new_it != new_msg->end();
       ++orig_it, ++new_it) {
    EXPECT_THAT((*orig_it)->GetPacketPayload(),
                testing::ContainerEq((*new_it)->GetPacketPayload()));

    EXPECT_EQ((*orig_it)->channel_id(), (*new_it)->channel_id());

    ASSERT_EQ((*orig_it)->GetSerializedBuffer()->size(),
              (*new_it)->GetSerializedBuffer()->size());
    for (size_t i = 0;
         i < static_cast<size_t>((*new_it)->GetSerializedBuffer()->size());
         ++i) {
      EXPECT_EQ((*orig_it)->GetSerializedBuffer()->data()[i],
                (*new_it)->GetSerializedBuffer()->data()[i]);
    }
  }
}

TEST_F(U2fMessageTest, TestMessagePartitoning) {
  uint32_t channel_id = 0x01010203;
  std::vector<uint8_t> data(U2fMessage::kInitPacketDataSize + 1);
  scoped_refptr<U2fMessage> two_packet_message =
      U2fMessage::Create(channel_id, U2fMessage::Type::CMD_PING, data);
  EXPECT_EQ(2U, two_packet_message->NumPackets());

  data.resize(U2fMessage::kInitPacketDataSize);
  scoped_refptr<U2fMessage> one_packet_message =
      U2fMessage::Create(channel_id, U2fMessage::Type::CMD_PING, data);
  EXPECT_EQ(1U, one_packet_message->NumPackets());

  data.resize(U2fMessage::kInitPacketDataSize +
              U2fMessage::kContinuationPacketDataSize + 1);
  scoped_refptr<U2fMessage> three_packet_message =
      U2fMessage::Create(channel_id, U2fMessage::Type::CMD_PING, data);
  EXPECT_EQ(3U, three_packet_message->NumPackets());
}

TEST_F(U2fMessageTest, TestMaxSize) {
  uint32_t channel_id = 0x00010203;
  std::vector<uint8_t> data(U2fMessage::kMaxMessageSize + 1);
  scoped_refptr<U2fMessage> oversize_message =
      U2fMessage::Create(channel_id, U2fMessage::Type::CMD_PING, data);
  EXPECT_EQ(nullptr, oversize_message);
}

TEST_F(U2fMessageTest, TestDeconstruct) {
  uint32_t channel_id = 0x0A0B0C0D;
  std::vector<uint8_t> data(U2fMessage::kMaxMessageSize, 0x7F);
  scoped_refptr<U2fMessage> filled_message =
      U2fMessage::Create(channel_id, U2fMessage::Type::CMD_PING, data);

  EXPECT_THAT(data, testing::ContainerEq(filled_message->GetMessagePayload()));
}

TEST_F(U2fMessageTest, TestDeserialize) {
  uint32_t channel_id = 0x0A0B0C0D;
  std::vector<uint8_t> data(U2fMessage::kMaxMessageSize);

  scoped_refptr<U2fMessage> orig_message =
      U2fMessage::Create(channel_id, U2fMessage::Type::CMD_PING, data);
  std::list<scoped_refptr<net::IOBufferWithSize>> orig_list;
  scoped_refptr<net::IOBufferWithSize> buf = orig_message->PopNextPacket();
  orig_list.push_back(buf);

  scoped_refptr<U2fMessage> new_message =
      U2fMessage::CreateFromSerializedData(buf);
  while (!new_message->MessageComplete()) {
    buf = orig_message->PopNextPacket();
    orig_list.push_back(buf);
    new_message->AddContinuationPacket(buf);
  }

  while ((buf = new_message->PopNextPacket())) {
    ASSERT_EQ(buf->size(), orig_list.front()->size());
    EXPECT_EQ(0, memcmp(buf->data(), orig_list.front()->data(), buf->size()));
    orig_list.pop_front();
  }
}
}  // namespace device
