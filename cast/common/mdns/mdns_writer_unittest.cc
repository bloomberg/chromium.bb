// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_writer.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace cast {
namespace mdns {

TEST(MdnsWriterTest, WriteDomainName) {
  // clang-format off
  constexpr uint8_t kExpectedResult[] = {
    0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00
  };
  // clang-format on
  uint8_t result[sizeof(kExpectedResult)];
  MdnsWriter writer(result, sizeof(kExpectedResult));
  ASSERT_TRUE(writer.WriteDomainName(DomainName{"testing", "local"}));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedResult, result, sizeof(result)));
}

TEST(MdnsWriterTest, WriteDomainName_CompressedMessage) {
  // clang-format off
  constexpr uint8_t kExpectedResultCompressed[] = {
    0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,
    0x06, 'p', 'r', 'e', 'f', 'i', 'x',
    0xC0, 0x08,  // byte 8
    0x03, 'n', 'e', 'w',
    0xC0, 0x0F,  // byte 15
    0xC0, 0x0F,  // byte 15
  };
  // clang-format on
  uint8_t result[sizeof(kExpectedResultCompressed)];
  MdnsWriter writer(result, sizeof(kExpectedResultCompressed));
  ASSERT_TRUE(writer.WriteDomainName(DomainName{"testing", "local"}));
  ASSERT_TRUE(writer.WriteDomainName(DomainName{"prefix", "local"}));
  ASSERT_TRUE(writer.WriteDomainName(DomainName{"new", "prefix", "local"}));
  ASSERT_TRUE(writer.WriteDomainName(DomainName{"prefix", "local"}));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_THAT(std::vector<uint8_t>(result, result + sizeof(result)),
              testing::ElementsAreArray(kExpectedResultCompressed));
}

TEST(MdnsWriterTest, WriteDomainName_NotEnoughSpace) {
  // clang-format off
  constexpr uint8_t kExpectedResultCompressed[] = {
    0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,
    0x09, 'd', 'i', 'f', 'f', 'e', 'r', 'e', 'n', 't',
    0x06, 'd', 'o', 'm', 'a', 'i', 'n',
    0x00
  };
  // clang-format on
  uint8_t result[sizeof(kExpectedResultCompressed)];
  MdnsWriter writer(result, sizeof(kExpectedResultCompressed));
  ASSERT_TRUE(writer.WriteDomainName(DomainName{"testing", "local"}));
  // Not enough space to write this domain name. Failure to write it must not
  // affect correct successful write of the next domain name.
  ASSERT_FALSE(writer.WriteDomainName(DomainName{"a", "different", "domain"}));
  ASSERT_TRUE(writer.WriteDomainName(DomainName{"different", "domain"}));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_THAT(std::vector<uint8_t>(result, result + sizeof(result)),
              testing::ElementsAreArray(kExpectedResultCompressed));
}

TEST(MdnsWriterTest, WriteDomainName_Long) {
  constexpr char kLongLabel[] =
      "12345678901234567890123456789012345678901234567890";
  // clang-format off
  constexpr uint8_t kExpectedResult[] = {
      0x32, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3',
            '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6',
            '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
      0x32, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3',
            '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6',
            '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
      0x32, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3',
            '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6',
            '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
      0x32, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '1', '2', '3',
            '4', '5', '6', '7', '8', '9', '0', '1', '2', '3', '4', '5', '6',
            '7', '8', '9', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0',
      0x00,
  };
  // clang-format on
  DomainName name{kLongLabel, kLongLabel, kLongLabel, kLongLabel};
  uint8_t result[sizeof(kExpectedResult)];
  MdnsWriter writer(result, sizeof(kExpectedResult));
  ASSERT_TRUE(writer.WriteDomainName(name));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedResult, result, sizeof(result)));
}

TEST(MdnsWriterTest, WriteDomainName_Empty) {
  DomainName name;
  uint8_t result[256];
  MdnsWriter writer(result, sizeof(result));
  EXPECT_FALSE(writer.WriteDomainName(name));
  // The writer should not have moved its internal pointer when it fails to
  // write. It should fail without any side effects.
  EXPECT_EQ(0u, writer.offset());
}

TEST(MdnsWriterTest, WriteDomainName_NoCompressionForBigOffsets) {
  // clang-format off
  constexpr uint8_t kExpectedResultCompressed[] = {
    0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,
    0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,
  };
  // clang-format on

  DomainName name{"testing", "local"};
  // Maximum supported value for label pointer offset is 0x3FFF.
  // Labels written into a buffer at greater offsets must not
  // produce compression label pointers.
  std::vector<uint8_t> buffer(0x4000 + sizeof(kExpectedResultCompressed));
  {
    MdnsWriter writer(buffer.data(), buffer.size());
    writer.Skip(0x4000);
    ASSERT_TRUE(writer.WriteDomainName(name));
    ASSERT_TRUE(writer.WriteDomainName(name));
    EXPECT_EQ(0UL, writer.remaining());
  }
  buffer.erase(buffer.begin(), buffer.begin() + 0x4000);
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedResultCompressed));
}

}  // namespace mdns
}  // namespace cast
