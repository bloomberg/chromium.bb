// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/common/mdns/mdns_records.h"

#include "cast/common/mdns/mdns_reader.h"
#include "cast/common/mdns/mdns_writer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/network_interface.h"

namespace cast {
namespace mdns {

TEST(MdnsDomainNameTest, Labels) {
  DomainName name{"MyDevice", "_mYSERvice", "local"};
  ASSERT_EQ(3U, name.label_count());
  EXPECT_EQ("MyDevice", name.Label(0));
  EXPECT_EQ("_mYSERvice", name.Label(1));
  EXPECT_EQ("local", name.Label(2));
  EXPECT_EQ("MyDevice._mYSERvice.local", name.ToString());

  DomainName other_name{"OtherDevice", "_MYservice", "LOcal"};
  ASSERT_EQ(3U, other_name.label_count());
  EXPECT_EQ("OtherDevice", other_name.Label(0));
  EXPECT_EQ("_MYservice", other_name.Label(1));
  EXPECT_EQ("LOcal", other_name.Label(2));
  EXPECT_EQ("OtherDevice._MYservice.LOcal", other_name.ToString());
}

TEST(MdnsDomainNameTest, CopyAndAssign) {
  DomainName name{"testing", "local"};
  EXPECT_EQ(15u, name.max_wire_size());

  DomainName name_copy(name);
  EXPECT_EQ(name_copy, name);
  EXPECT_EQ(15u, name_copy.max_wire_size());

  DomainName name_assign = name;
  EXPECT_EQ(name_assign, name);
  EXPECT_EQ(15u, name_assign.max_wire_size());
}

TEST(MdnsDomainNameTest, IsEqual) {
  DomainName first{"testing", "local"};
  DomainName second{"TeStInG", "LOCAL"};
  DomainName third{"testing"};
  DomainName fourth{"testing.local"};
  DomainName fifth{"Testing.Local"};

  EXPECT_EQ(first, second);
  EXPECT_EQ(fourth, fifth);
  EXPECT_NE(first, third);
  EXPECT_NE(first, fourth);
}

TEST(MdnsRdataTest, SrvRecordRdata) {
  constexpr uint8_t kExpectedRdata[] = {
      0x00, 0x15,  // RDLENGTH = 21
      0x00, 0x05,  // PRIORITY = 5
      0x00, 0x06,  // WEIGHT = 6
      0x1f, 0x49,  // PORT = 8009
      0x07, 't',  'e', 's', 't', 'i', 'n',  'g',
      0x05, 'l',  'o', 'c', 'a', 'l', 0x00,
  };
  SrvRecordRdata rdata(5, 6, 8009, DomainName{"testing", "local"});
  // RDLENGTH is uint16_t and is a part of kExpectedRdata.
  EXPECT_EQ(sizeof(kExpectedRdata), rdata.max_wire_size() + sizeof(uint16_t));

  MdnsReader reader(kExpectedRdata, sizeof(kExpectedRdata));
  SrvRecordRdata rdata_read;
  EXPECT_TRUE(reader.ReadSrvRecordRdata(&rdata_read));
  EXPECT_EQ(rdata_read, rdata);
  EXPECT_EQ(0UL, reader.remaining());

  uint8_t buffer[sizeof(kExpectedRdata)];
  MdnsWriter writer(buffer, sizeof(buffer));
  EXPECT_TRUE(writer.WriteSrvRecordRdata(rdata));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedRdata, buffer, sizeof(buffer)));
}

TEST(MdnsRdataTest, ARecordRdata) {
  constexpr uint8_t kExpectedRdata[] = {
      0x00, 0x4,               // RDLENGTH = 4
      0x08, 0x08, 0x08, 0x08,  // ADDRESS = 8.8.8.8
  };
  auto parse_result = IPAddress::Parse("8.8.8.8");
  ASSERT_TRUE(parse_result);
  ARecordRdata rdata(parse_result.MoveValue());
  // RDLENGTH is uint16_t and is a part of kExpectedRdata.
  EXPECT_EQ(sizeof(kExpectedRdata), rdata.max_wire_size() + sizeof(uint16_t));

  MdnsReader reader(kExpectedRdata, sizeof(kExpectedRdata));
  ARecordRdata rdata_read;
  EXPECT_TRUE(reader.ReadARecordRdata(&rdata_read));
  EXPECT_TRUE(rdata_read.ipv4_address().IsV4());
  EXPECT_EQ(rdata_read, rdata);
  EXPECT_EQ(0UL, reader.remaining());

  uint8_t buffer[sizeof(kExpectedRdata)];
  MdnsWriter writer(buffer, sizeof(buffer));
  EXPECT_TRUE(writer.WriteARecordRdata(rdata));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedRdata, buffer, sizeof(buffer)));
}

TEST(MdnsRdataTest, AAAARecordRdata) {
  // clang-format off
  constexpr uint8_t kExpectedRdata[] = {
      0x00, 0x10,  // RDLENGTH = 16
      // ADDRESS = FE80:0000:0000:0000:0202:B3FF:FE1E:8329
      0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x02, 0x02, 0xb3, 0xff, 0xfe, 0x1e, 0x83, 0x29,
  };
  // clang-format on
  auto parse_result =
      IPAddress::Parse("FE80:0000:0000:0000:0202:B3FF:FE1E:8329");
  ASSERT_TRUE(parse_result);
  AAAARecordRdata rdata(parse_result.MoveValue());
  // RDLENGTH is uint16_t and is a part of kExpectedRdata.
  EXPECT_EQ(sizeof(kExpectedRdata), rdata.max_wire_size() + sizeof(uint16_t));

  MdnsReader reader(kExpectedRdata, sizeof(kExpectedRdata));
  AAAARecordRdata rdata_read;
  EXPECT_TRUE(reader.ReadAAAARecordRdata(&rdata_read));
  EXPECT_TRUE(rdata_read.ipv6_address().IsV6());
  EXPECT_EQ(rdata_read, rdata);
  EXPECT_EQ(0UL, reader.remaining());

  uint8_t buffer[sizeof(kExpectedRdata)];
  MdnsWriter writer(buffer, sizeof(buffer));
  EXPECT_TRUE(writer.WriteAAAARecordRdata(rdata));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedRdata, buffer, sizeof(buffer)));
}

TEST(MdnsRdataTest, PtrRecordRdata) {
  // clang-format off
  constexpr uint8_t kExpectedRdata[] = {
      0x00, 0x18,  // RDLENGTH = 24
      0x08, 'm', 'y', 'd', 'e', 'v',  'i', 'c', 'e',
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a',  'l',
      0x00,
  };
  // clang-format on
  PtrRecordRdata rdata(DomainName{"mydevice", "testing", "local"});
  // RDLENGTH is uint16_t and is a part of kExpectedRdata.
  EXPECT_EQ(sizeof(kExpectedRdata), rdata.max_wire_size() + sizeof(uint16_t));

  MdnsReader reader(kExpectedRdata, sizeof(kExpectedRdata));
  PtrRecordRdata rdata_read;
  EXPECT_TRUE(reader.ReadPtrRecordRdata(&rdata_read));
  EXPECT_EQ(rdata_read, rdata);
  EXPECT_EQ(0UL, reader.remaining());

  uint8_t buffer[sizeof(kExpectedRdata)];
  MdnsWriter writer(buffer, sizeof(buffer));
  EXPECT_TRUE(writer.WritePtrRecordRdata(rdata));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedRdata, buffer, sizeof(buffer)));
}

TEST(MdnsRdataTest, TxtRecordRdata) {
  // clang-format off
  constexpr uint8_t kExpectedRdata[] = {
      0x00, 0x1B,  // RDLENGTH = 27
      0x05, 'f', 'o', 'o', '=', '1',
      0x05, 'b', 'a', 'r', '=', '2',
      0x0e, 'n', 'a', 'm', 'e', '=',
      'w', 'e', '.',  '.', 'i', 'r', 'd', '/', '/',
  };
  // clang-format on
  std::vector<std::string> texts;
  texts.push_back("foo=1");
  texts.push_back("bar=2");
  texts.push_back("name=we..ird//");
  TxtRecordRdata rdata(std::move(texts));
  // RDLENGTH is uint16_t and is a part of kExpectedRdata.
  EXPECT_EQ(sizeof(kExpectedRdata), rdata.max_wire_size() + sizeof(uint16_t));

  MdnsReader reader(kExpectedRdata, sizeof(kExpectedRdata));
  TxtRecordRdata rdata_read;
  EXPECT_TRUE(reader.ReadTxtRecordRdata(&rdata_read));
  EXPECT_EQ(rdata_read, rdata);
  EXPECT_EQ(0UL, reader.remaining());

  uint8_t buffer[sizeof(kExpectedRdata)];
  MdnsWriter writer(buffer, sizeof(buffer));
  EXPECT_TRUE(writer.WriteTxtRecordRdata(rdata));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedRdata, buffer, sizeof(buffer)));
}

TEST(MdnsRdataTest, TxtRecordRdata_Empty) {
  constexpr uint8_t kExpectedRdata[] = {
      0x00, 0x01,  // RDLENGTH = 1
      0x00,        // empty string
  };
  std::vector<std::string> texts;
  TxtRecordRdata rdata(std::move(texts));
  // RDLENGTH is uint16_t and is a part of kExpectedRdata.
  EXPECT_EQ(sizeof(kExpectedRdata), rdata.max_wire_size() + sizeof(uint16_t));

  MdnsReader reader(kExpectedRdata, sizeof(kExpectedRdata));
  TxtRecordRdata rdata_read;
  EXPECT_TRUE(reader.ReadTxtRecordRdata(&rdata_read));
  EXPECT_EQ(rdata_read, rdata);
  EXPECT_EQ(0UL, reader.remaining());

  uint8_t buffer[sizeof(kExpectedRdata)];
  MdnsWriter writer(buffer, sizeof(buffer));
  EXPECT_TRUE(writer.WriteTxtRecordRdata(rdata));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedRdata, buffer, sizeof(buffer)));
}

TEST(MdnsRdataTest, IsEqual) {
  auto parse_result = IPAddress::Parse("8.8.8.8");
  ASSERT_TRUE(parse_result);
  ARecordRdata a_rdata(parse_result.value());
  ARecordRdata a_rdata_2(parse_result.value());
  EXPECT_EQ(a_rdata, a_rdata_2);
}

TEST(MdnsRecordTest, CopyAndAssign) {
  MdnsRecord record(DomainName{"hostname", "local"}, kTypePTR,
                    kClassIN | kCacheFlushBit, 120,
                    PtrRecordRdata(DomainName{"testing", "local"}));

  MdnsRecord record_copy(record);
  EXPECT_EQ(record.name(), record_copy.name());
  EXPECT_EQ(record.type(), record_copy.type());
  EXPECT_EQ(record.record_class(), record_copy.record_class());
  EXPECT_EQ(record.ttl(), record_copy.ttl());
  EXPECT_EQ(record.rdata(), record_copy.rdata());
  EXPECT_EQ(record, record_copy);

  MdnsRecord record_assign;
  record_assign = record;
  EXPECT_EQ(record.name(), record_assign.name());
  EXPECT_EQ(record.type(), record_assign.type());
  EXPECT_EQ(record.record_class(), record_copy.record_class());
  EXPECT_EQ(record.ttl(), record_assign.ttl());
  EXPECT_EQ(record.rdata(), record_copy.rdata());
  EXPECT_EQ(record, record_assign);
}

TEST(MdnsRecordTest, ReadARecord) {
  // clang-format off
  const uint8_t kTestRecord[] = {
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x01,              // TYPE = A (1)
      0x80, 0x01,              // CLASS = IN (1) | CACHE_FLUSH_BIT
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x04,              // RDLENGTH = 4 bytes
      0x08, 0x08, 0x08, 0x08,  // RDATA = 8.8.8.8
  };
  // clang-format on
  MdnsReader reader(kTestRecord, sizeof(kTestRecord));
  MdnsRecord record;
  EXPECT_TRUE(reader.ReadMdnsRecord(&record));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));

  EXPECT_EQ(record.name().ToString(), "testing.local");
  EXPECT_EQ(record.type(), kTypeA);
  EXPECT_EQ(record.record_class(), kClassIN | kCacheFlushBit);
  EXPECT_EQ(record.ttl(), UINT32_C(120));
  ARecordRdata a_rdata(IPAddress{8, 8, 8, 8});
  EXPECT_EQ(record.rdata(), Rdata(a_rdata));
}

TEST(MdnsRecordTest, ReadUnknownRecordType) {
  // clang-format off
  const uint8_t kTestRecord[] = {
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x05,              // TYPE = CNAME (5)
      0x80, 0x01,              // CLASS = IN (1) | CACHE_FLUSH_BIT
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x08,              // RDLENGTH = 8 bytes
      0x05, 'c', 'n', 'a', 'm', 'e', 0xc0, 0x00,
  };
  const uint8_t kCnameRdata[] = {
      0x05, 'c', 'n', 'a', 'm', 'e', 0xc0, 0x00,
  };
  // clang-format on
  MdnsReader reader(kTestRecord, sizeof(kTestRecord));
  MdnsRecord record;
  EXPECT_TRUE(reader.ReadMdnsRecord(&record));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));

  EXPECT_EQ(record.name().ToString(), "testing.local");
  EXPECT_EQ(record.type(), kTypeCNAME);
  EXPECT_EQ(record.record_class(), kClassIN | kCacheFlushBit);
  EXPECT_EQ(record.ttl(), UINT32_C(120));
  RawRecordRdata raw_rdata(kCnameRdata, sizeof(kCnameRdata));
  EXPECT_EQ(record.rdata(), Rdata(raw_rdata));
}

TEST(MdnsRecordTest, ReadCompressedNames) {
  // clang-format off
  const uint8_t kTestRecord[] = {
      // First message
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x0c,              // TYPE = PTR (12)
      0x00, 0x01,              // CLASS = IN (1)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x06,              // RDLENGTH = 6 bytes
      0x03, 'p',  't',  'r',
      0xc0, 0x00,              // Domain name label pointer to byte 0
      // Second message
      0x03, 'o', 'n', 'e',
      0x03, 't', 'w', 'o',
      0xc0, 0x00,              // Domain name label pointer to byte 0
      0x00, 0x01,              // TYPE = A (1)
      0x80, 0x01,              // CLASS = IN (1) | CACHE_FLUSH_BIT
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x04,              // RDLENGTH = 4 bytes
      0x08, 0x08, 0x08, 0x08,  // RDATA = 8.8.8.8
  };
  // clang-format on
  MdnsReader reader(kTestRecord, sizeof(kTestRecord));

  MdnsRecord record;
  EXPECT_TRUE(reader.ReadMdnsRecord(&record));

  EXPECT_EQ(record.name().ToString(), "testing.local");
  EXPECT_EQ(record.type(), kTypePTR);
  EXPECT_EQ(record.record_class(), kClassIN);
  EXPECT_EQ(record.ttl(), UINT32_C(120));
  PtrRecordRdata ptr_rdata(DomainName{"ptr", "testing", "local"});
  EXPECT_EQ(record.rdata(), Rdata(ptr_rdata));

  EXPECT_TRUE(reader.ReadMdnsRecord(&record));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));

  EXPECT_EQ(record.name().ToString(), "one.two.testing.local");
  EXPECT_EQ(record.type(), kTypeA);
  EXPECT_EQ(record.record_class(), kClassIN | kCacheFlushBit);
  EXPECT_EQ(record.ttl(), UINT32_C(120));
  ARecordRdata a_rdata(IPAddress{8, 8, 8, 8});
  EXPECT_EQ(record.rdata(), Rdata(a_rdata));
}

TEST(MdnsRecordTest, FailToReadMissingRdata) {
  // clang-format off
  const uint8_t kTestRecord[] = {
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x01,              // TYPE = A (1)
      0x00, 0x01,              // CLASS = IN (1)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x04,              // RDLENGTH = 4 bytes
                               // Missing RDATA
  };
  // clang-format on
  MdnsReader reader(kTestRecord, sizeof(kTestRecord));
  MdnsRecord record;
  EXPECT_FALSE(reader.ReadMdnsRecord(&record));
}

TEST(MdnsRecordTest, FailToReadInvalidHostName) {
  // clang-format off
  const uint8_t kTestRecord[] = {
      // Invalid NAME: length byte too short
      0x03, 'i', 'n', 'v', 'a', 'l', 'i', 'd',
      0x00,
      0x00, 0x01,              // TYPE = A (1)
      0x00, 0x01,              // CLASS = IN (1)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x04,              // RDLENGTH = 4 bytes
      0x08, 0x08, 0x08, 0x08,  // RDATA = 8.8.8.8
  };
  // clang-format on
  MdnsReader reader(kTestRecord, sizeof(kTestRecord));
  MdnsRecord record;
  EXPECT_FALSE(reader.ReadMdnsRecord(&record));
}

TEST(MdnsRecordTest, WriteARecord) {
  // clang-format off
  const uint8_t kExpectedResult[] = {
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x01,              // TYPE = A (1)
      0x80, 0x01,              // CLASS = IN (1) | CACHE_FLUSH_BIT
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x04,              // RDLENGTH = 4 bytes
      0xac, 0x00, 0x00, 0x01,  // 172.0.0.1
  };
  // clang-format on
  MdnsRecord record(DomainName{"testing", "local"}, kTypeA,
                    kClassIN | kCacheFlushBit, 120,
                    ARecordRdata(IPAddress{172, 0, 0, 1}));

  std::vector<uint8_t> buffer(sizeof(kExpectedResult));
  MdnsWriter writer(buffer.data(), buffer.size());
  EXPECT_TRUE(writer.WriteMdnsRecord(record));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedResult));
}

TEST(MdnsRecordTest, WritePtrRecord) {
  // clang-format off
  const uint8_t kExpectedResult[] = {
      0x08, '_', 's', 'e', 'r', 'v', 'i', 'c', 'e',
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x0c,              // TYPE = PTR (12)
      0x00, 0x01,              // CLASS = IN (1)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x02,              // RDLENGTH = 2 bytes
      0xc0, 0x09,              // Domain name label pointer to byte
  };
  // clang-format on
  MdnsRecord record(DomainName{"_service", "testing", "local"}, kTypePTR,
                    kClassIN, 120,
                    PtrRecordRdata(DomainName{"testing", "local"}));

  std::vector<uint8_t> buffer(sizeof(kExpectedResult));
  MdnsWriter writer(buffer.data(), buffer.size());
  EXPECT_TRUE(writer.WriteMdnsRecord(record));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedResult));
}

}  // namespace mdns
}  // namespace cast
