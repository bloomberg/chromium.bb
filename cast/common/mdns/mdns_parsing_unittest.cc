// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_parsing.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace cast {
namespace mdns {

TEST(MdnsDomainNameTest, BasicDomainNames) {
  DomainName name;
  EXPECT_TRUE(name.PushLabel("MyDevice"));
  EXPECT_TRUE(name.PushLabel("_mYSERvice"));
  EXPECT_TRUE(name.PushLabel("local"));
  ASSERT_EQ(3U, name.label_count());
  EXPECT_EQ("MyDevice", name.Label(0));
  EXPECT_EQ("_mYSERvice", name.Label(1));
  EXPECT_EQ("local", name.Label(2));
  EXPECT_EQ("MyDevice._mYSERvice.local", name.ToString());

  DomainName other_name;
  EXPECT_TRUE(other_name.PushLabel("OtherDevice"));
  EXPECT_TRUE(other_name.PushLabel("_MYservice"));
  EXPECT_TRUE(other_name.PushLabel("LOcal"));
  ASSERT_EQ(3U, other_name.label_count());
  EXPECT_EQ("OtherDevice", other_name.Label(0));
  EXPECT_EQ("_MYservice", other_name.Label(1));
  EXPECT_EQ("LOcal", other_name.Label(2));
  EXPECT_EQ("OtherDevice._MYservice.LOcal", other_name.ToString());
}

TEST(MdnsDomainNameTest, CopyAndAssignAndClear) {
  DomainName name;
  name.PushLabel("testing");
  name.PushLabel("local");
  EXPECT_EQ(15u, name.max_wire_size());

  DomainName name_copy(name);
  EXPECT_EQ(name_copy, name);
  EXPECT_EQ(15u, name_copy.max_wire_size());

  DomainName name_assign = name;
  EXPECT_EQ(name_assign, name);
  EXPECT_EQ(15u, name_assign.max_wire_size());

  name.Clear();
  EXPECT_EQ(1u, name.max_wire_size());
  EXPECT_NE(name_copy, name);
  EXPECT_NE(name_assign, name);
  EXPECT_EQ(name_copy, name_assign);
}

TEST(MdnsDomainNameTest, IsEqual) {
  DomainName first;
  first.PushLabel("testing");
  first.PushLabel("local");
  DomainName second;
  second.PushLabel("TeStInG");
  second.PushLabel("LOCAL");
  DomainName third;
  third.PushLabel("testing");
  DomainName fourth;
  fourth.PushLabel("testing.local");
  DomainName fifth;
  fifth.PushLabel("Testing.Local");

  EXPECT_EQ(first, second);
  EXPECT_EQ(fourth, fifth);

  EXPECT_NE(first, third);
  EXPECT_NE(first, fourth);
}

TEST(MdnsDomainNameTest, PushLabel_InvalidLabels) {
  DomainName name;
  EXPECT_TRUE(name.PushLabel("testing"));
  EXPECT_FALSE(name.PushLabel(""));                    // Empty label
  EXPECT_FALSE(name.PushLabel(std::string(64, 'a')));  // Label too long
}

TEST(MdnsDomainNameTest, PushLabel_NameTooLong) {
  std::string maximum_label(63, 'a');

  DomainName name;
  EXPECT_TRUE(name.PushLabel(maximum_label));         // 64 bytes
  EXPECT_TRUE(name.PushLabel(maximum_label));         // 128 bytes
  EXPECT_TRUE(name.PushLabel(maximum_label));         // 192 bytes
  EXPECT_FALSE(name.PushLabel(maximum_label));        // NAME > 255 bytes
  EXPECT_TRUE(name.PushLabel(std::string(62, 'a')));  // NAME = 255
  EXPECT_EQ(256u, name.max_wire_size());
}

TEST(MdnsReaderTest, ReadDomainName) {
  const uint8_t kMessage[] = {
      // First name
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',  // Byte: 0
      0x05, 'l', 'o', 'c', 'a', 'l',            // Byte: 8
      0x00,                                     // Byte: 14
      // Second name
      0x07, 's', 'e', 'r', 'v', 'i', 'c', 'e',  // Byte: 15
      0xc0, 0x00,                               // Byte: 23
      // Third name
      0x06, 'd', 'e', 'v', 'i', 'c', 'e',  // Byte: 25
      0xc0, 0x0f,                          // Byte: 32
      // Fourth name
      0xc0, 0x20,  // Byte: 34
  };
  MdnsReader reader(kMessage, sizeof(kMessage));
  EXPECT_EQ(reader.begin(), kMessage);
  EXPECT_EQ(reader.length(), sizeof(kMessage));
  EXPECT_EQ(reader.offset(), 0u);
  DomainName name;
  EXPECT_TRUE(reader.ReadDomainName(&name));
  EXPECT_EQ(name.ToString(), "testing.local");
  EXPECT_TRUE(reader.ReadDomainName(&name));
  EXPECT_EQ(name.ToString(), "service.testing.local");
  EXPECT_TRUE(reader.ReadDomainName(&name));
  EXPECT_EQ(name.ToString(), "device.service.testing.local");
  EXPECT_TRUE(reader.ReadDomainName(&name));
  EXPECT_EQ(name.ToString(), "service.testing.local");
  EXPECT_EQ(reader.offset(), sizeof(kMessage));
  EXPECT_EQ(0UL, reader.remaining());
  EXPECT_FALSE(reader.ReadDomainName(&name));
}

TEST(MdnsReaderTest, ReadDomainName_Empty) {
  const uint8_t kDomainName[] = {0x00};
  MdnsReader reader(kDomainName, sizeof(kDomainName));
  DomainName name;
  EXPECT_TRUE(reader.ReadDomainName(&name));
  EXPECT_EQ(0UL, reader.remaining());
}

// There should be no side effects for failing to read a domain name.
// The underlying pointer should not have changed.

TEST(MdnsReaderTest, ReadDomainName_TooShort) {
  const uint8_t kDomainName[] = {0x03, 'a', 'b'};
  MdnsReader reader(kDomainName, sizeof(kDomainName));
  DomainName name;
  EXPECT_FALSE(reader.ReadDomainName(&name));
  EXPECT_EQ(0u, reader.offset());
}

TEST(MdnsReaderTest, ReadDomainName_TooLong) {
  std::vector<uint8_t> kDomainName;
  for (uint8_t letter = 'a'; letter <= 'z'; ++letter) {
    const uint8_t repetitions = 10;
    kDomainName.push_back(repetitions);
    kDomainName.insert(kDomainName.end(), repetitions, letter);
  }
  kDomainName.push_back(0);

  MdnsReader reader(kDomainName.data(), kDomainName.size());
  DomainName name;
  EXPECT_FALSE(reader.ReadDomainName(&name));
  EXPECT_EQ(0u, reader.offset());
}

TEST(MdnsReaderTest, ReadDomainName_LabelPointerOutOfBounds) {
  const uint8_t kDomainName[] = {0xc0, 0x02};
  MdnsReader reader(kDomainName, sizeof(kDomainName));
  DomainName name;
  EXPECT_FALSE(reader.ReadDomainName(&name));
  EXPECT_EQ(0u, reader.offset());
}

TEST(MdnsReaderTest, ReadDomainName_InvalidLabel) {
  const uint8_t kDomainName[] = {0x80};
  MdnsReader reader(kDomainName, sizeof(kDomainName));
  DomainName name;
  EXPECT_FALSE(reader.ReadDomainName(&name));
  EXPECT_EQ(0u, reader.offset());
}

TEST(MdnsReaderTest, ReadDomainName_CircularCompression) {
  // clang-format off
  const uint8_t kDomainName[] = {
      // NOTE: Circular label pointer at end of name.
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',  // Byte: 0
      0x05, 'l', 'o', 'c', 'a', 'l',            // Byte: 8
      0xc0, 0x00,                               // Byte: 14
  };
  // clang-format on
  MdnsReader reader(kDomainName, sizeof(kDomainName));
  DomainName name;
  EXPECT_FALSE(reader.ReadDomainName(&name));
  EXPECT_EQ(0u, reader.offset());
}

TEST(MdnsWriterTest, WriteDomainName_Simple) {
  // clang-format off
  const uint8_t kExpectedResult[] = {
    0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00
  };
  // clang-format on
  DomainName name;
  EXPECT_TRUE(name.PushLabel("testing"));
  EXPECT_TRUE(name.PushLabel("local"));
  uint8_t result[sizeof(kExpectedResult)];
  MdnsWriter writer(result, sizeof(kExpectedResult));
  ASSERT_TRUE(writer.WriteDomainName(name));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedResult, result, sizeof(result)));
}

TEST(MdnsWriterTest, WriteDomainName_CompressedMessage) {
  // clang-format off
  const uint8_t kExpectedResultCompressed[] = {
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
  DomainName name1;
  name1.PushLabel("testing");
  name1.PushLabel("local");

  DomainName name2;
  name2.PushLabel("prefix");
  name2.PushLabel("local");

  DomainName name3;
  name3.PushLabel("new");
  name3.PushLabel("prefix");
  name3.PushLabel("local");

  DomainName name4;
  name4.PushLabel("prefix");
  name4.PushLabel("local");

  uint8_t result[sizeof(kExpectedResultCompressed)];
  MdnsWriter writer(result, sizeof(kExpectedResultCompressed));
  ASSERT_TRUE(writer.WriteDomainName(name1));
  ASSERT_TRUE(writer.WriteDomainName(name2));
  ASSERT_TRUE(writer.WriteDomainName(name3));
  ASSERT_TRUE(writer.WriteDomainName(name4));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_THAT(std::vector<uint8_t>(result, result + sizeof(result)),
              testing::ElementsAreArray(kExpectedResultCompressed));
}

TEST(MdnsWriterTest, WriteDomainName_NotEnoughSpace) {
  // clang-format off
  const uint8_t kExpectedResultCompressed[] = {
    0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,
    0x09, 'd', 'i', 'f', 'f', 'e', 'r', 'e', 'n', 't',
    0x06, 'd', 'o', 'm', 'a', 'i', 'n',
    0x00
  };
  // clang-format on
  DomainName name1;
  name1.PushLabel("testing");
  name1.PushLabel("local");

  // Not enough space to write this domain name. Failure to write it must not
  // affect correct successful write of the next domain name.
  DomainName name2;
  name2.PushLabel("some");
  name2.PushLabel("different");
  name2.PushLabel("domain");

  DomainName name3;
  name3.PushLabel("different");
  name3.PushLabel("domain");

  uint8_t result[sizeof(kExpectedResultCompressed)];
  MdnsWriter writer(result, sizeof(kExpectedResultCompressed));
  ASSERT_TRUE(writer.WriteDomainName(name1));
  ASSERT_FALSE(writer.WriteDomainName(name2));
  ASSERT_TRUE(writer.WriteDomainName(name3));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_THAT(std::vector<uint8_t>(result, result + sizeof(result)),
              testing::ElementsAreArray(kExpectedResultCompressed));
}

TEST(MdnsWriterTest, WriteDomainName_Long) {
  const char kLongLabel[] =
      "12345678901234567890123456789012345678901234567890";
  // clang-format off
  const uint8_t kExpectedResult[] = {
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
  DomainName name;
  EXPECT_TRUE(name.PushLabel(kLongLabel));
  EXPECT_TRUE(name.PushLabel(kLongLabel));
  EXPECT_TRUE(name.PushLabel(kLongLabel));
  EXPECT_TRUE(name.PushLabel(kLongLabel));
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
  const uint8_t kExpectedResultCompressed[] = {
    0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,
    0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
    0x05, 'l', 'o', 'c', 'a', 'l',
    0x00,
  };
  // clang-format on

  DomainName name;
  name.PushLabel("testing");
  name.PushLabel("local");

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
