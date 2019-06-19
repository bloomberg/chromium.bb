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
  EXPECT_TRUE(writer.Write(rdata));
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
  EXPECT_TRUE(writer.Write(rdata));
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
  EXPECT_TRUE(writer.Write(rdata));
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
  EXPECT_TRUE(writer.Write(rdata));
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
  TxtRecordRdata rdata{"foo=1", "bar=2", "name=we..ird//"};
  // RDLENGTH is uint16_t and is a part of kExpectedRdata.
  EXPECT_EQ(sizeof(kExpectedRdata), rdata.max_wire_size() + sizeof(uint16_t));

  MdnsReader reader(kExpectedRdata, sizeof(kExpectedRdata));
  TxtRecordRdata rdata_read;
  EXPECT_TRUE(reader.ReadTxtRecordRdata(&rdata_read));
  EXPECT_EQ(rdata_read, rdata);
  EXPECT_EQ(0UL, reader.remaining());

  uint8_t buffer[sizeof(kExpectedRdata)];
  MdnsWriter writer(buffer, sizeof(buffer));
  EXPECT_TRUE(writer.Write(rdata));
  EXPECT_EQ(0UL, writer.remaining());
  EXPECT_EQ(0, memcmp(kExpectedRdata, buffer, sizeof(buffer)));
}

TEST(MdnsRdataTest, TxtRecordRdata_Empty) {
  constexpr uint8_t kExpectedRdata[] = {
      0x00, 0x01,  // RDLENGTH = 1
      0x00,        // empty string
  };
  TxtRecordRdata rdata;
  // RDLENGTH is uint16_t and is a part of kExpectedRdata.
  EXPECT_EQ(sizeof(kExpectedRdata), rdata.max_wire_size() + sizeof(uint16_t));

  MdnsReader reader(kExpectedRdata, sizeof(kExpectedRdata));
  TxtRecordRdata rdata_read;
  EXPECT_TRUE(reader.ReadTxtRecordRdata(&rdata_read));
  EXPECT_EQ(rdata_read, rdata);
  EXPECT_EQ(0UL, reader.remaining());

  uint8_t buffer[sizeof(kExpectedRdata)];
  MdnsWriter writer(buffer, sizeof(buffer));
  EXPECT_TRUE(writer.Write(rdata));
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
  EXPECT_TRUE(writer.Write(record));
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
  EXPECT_TRUE(writer.Write(record));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedResult));
}

TEST(MdnsQuestionTest, CopyAndAssign) {
  MdnsQuestion question(DomainName{"testing", "local"}, kTypePTR,
                        kClassIN | kUnicastResponseBit);

  MdnsQuestion question_copy(question);
  EXPECT_EQ(question_copy.name().ToString(), "testing.local");
  EXPECT_EQ(question_copy.type(), kTypePTR);
  EXPECT_EQ(question_copy.record_class(), kClassIN | kUnicastResponseBit);
  EXPECT_EQ(question_copy, question);

  MdnsQuestion question_assign;
  question_assign = question;
  EXPECT_EQ(question_assign.name().ToString(), "testing.local");
  EXPECT_EQ(question_assign.type(), kTypePTR);
  EXPECT_EQ(question_assign.record_class(), kClassIN | kUnicastResponseBit);
  EXPECT_EQ(question_assign, question);
}

TEST(MdnsQuestionTest, Read) {
  // clang-format off
  const uint8_t kTestQuestion[] = {
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x01,  // TYPE = A (1)
      0x80, 0x01,  // CLASS = IN (1) | UNICAST_BIT
  };
  // clang-format on
  MdnsReader reader(kTestQuestion, sizeof(kTestQuestion));
  MdnsQuestion question;
  EXPECT_TRUE(reader.ReadMdnsQuestion(&question));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));

  EXPECT_EQ(question.name().ToString(), "testing.local");
  EXPECT_EQ(question.type(), kTypeA);
  EXPECT_EQ(question.record_class(), kClassIN | kUnicastResponseBit);
}

TEST(MdnsQuestionTest, ReadCompressedNames) {
  // clang-format off
  const uint8_t kTestQuestions[] = {
      // First Question
      0x05, 'f', 'i', 'r', 's', 't',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x01,  // TYPE = A (1)
      0x80, 0x01,  // CLASS = IN (1) | UNICAST_BIT
      // Second Question
      0x06, 's', 'e', 'c', 'o', 'n', 'd',
      0xc0, 0x06,  // Domain name label pointer
      0x00, 0x0c,  // TYPE = PTR (12)
      0x00, 0x01,  // CLASS = IN (1)
  };
  // clang-format on
  MdnsReader reader(kTestQuestions, sizeof(kTestQuestions));
  MdnsQuestion question;
  EXPECT_TRUE(reader.ReadMdnsQuestion(&question));

  EXPECT_EQ(question.name().ToString(), "first.local");
  EXPECT_EQ(question.type(), kTypeA);
  EXPECT_EQ(question.record_class(), kClassIN | kUnicastResponseBit);

  EXPECT_TRUE(reader.ReadMdnsQuestion(&question));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));

  EXPECT_EQ(question.name().ToString(), "second.local");
  EXPECT_EQ(question.type(), kTypePTR);
  EXPECT_EQ(question.record_class(), kClassIN);
}

TEST(MdnsQuestionTest, FailToReadInvalidHostName) {
  // clang-format off
  const uint8_t kTestQuestion[] = {
      // Invalid NAME: length byte too short
      0x03, 'i', 'n', 'v', 'a', 'l', 'i', 'd',
      0x00,
      0x00, 0x01,  // TYPE = A (1)
      0x00, 0x01,  // CLASS = IN (1)
  };
  // clang-format on
  MdnsReader reader(kTestQuestion, sizeof(kTestQuestion));
  MdnsQuestion question;
  EXPECT_FALSE(reader.ReadMdnsQuestion(&question));
}

TEST(MdnsQuestionTest, Write) {
  // clang-format off
  const uint8_t kExpectedResult[] = {
      0x04, 'w', 'i', 'r', 'e',
      0x06, 'f', 'o', 'r', 'm', 'a', 't',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x0c,  // TYPE = PTR (12)
      0x80, 0x01,  // CLASS = IN (1) | UNICAST_BIT
  };
  // clang-format on
  MdnsQuestion question(DomainName{"wire", "format", "local"}, kTypePTR,
                        kClassIN | kUnicastResponseBit);
  std::vector<uint8_t> buffer(sizeof(kExpectedResult));
  MdnsWriter writer(buffer.data(), buffer.size());
  EXPECT_TRUE(writer.Write(question));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedResult));
}

TEST(MdnsMessageTest, CopyAndAssign) {
  MdnsQuestion question(DomainName{"testing", "local"}, kTypePTR,
                        kClassIN | kUnicastResponseBit);

  MdnsRecord record1(DomainName{"record1"}, kTypeA, kClassIN, 120,
                     ARecordRdata(IPAddress{172, 0, 0, 1}));

  MdnsRecord record2(DomainName{"record2"}, kTypeTXT, kClassIN, 120,
                     TxtRecordRdata{"foo=1", "bar=2"});

  MdnsMessage message(123, 0x8400);
  message.AddQuestion(std::move(question));
  message.AddAnswer(std::move(record1));
  message.AddAnswer(std::move(record2));

  EXPECT_EQ(message.questions().size(), UINT64_C(1));
  EXPECT_EQ(message.answers().size(), UINT64_C(2));

  MdnsMessage message_copy(message);
  EXPECT_EQ(message_copy.id(), UINT16_C(123));
  EXPECT_EQ(message_copy.flags(), UINT16_C(0x8400));
  EXPECT_EQ(message_copy.questions(), message.questions());
  EXPECT_EQ(message_copy.answers(), message.answers());
  EXPECT_EQ(message_copy.authority_records(), message.authority_records());
  EXPECT_EQ(message_copy.additional_records(), message.additional_records());
  EXPECT_EQ(message_copy, message);

  MdnsMessage message_assign;
  message_assign = message;
  EXPECT_EQ(message_assign.id(), UINT16_C(123));
  EXPECT_EQ(message_assign.flags(), UINT16_C(0x8400));
  EXPECT_EQ(message_assign.questions(), message.questions());
  EXPECT_EQ(message_assign.answers(), message.answers());
  EXPECT_EQ(message_assign.authority_records(), message.authority_records());
  EXPECT_EQ(message_assign.additional_records(), message.additional_records());
  EXPECT_EQ(message_assign, message);
}

TEST(MdnsMessageTest, Read) {
  // clang-format off
  const uint8_t kTestMessage[] = {
      // Header
      0x00, 0x01,  // ID = 1
      0x84, 0x00,  // FLAGS = AA | RESPONSE
      0x00, 0x00,  // Questions = 0
      0x00, 0x01,  // Answers = 1
      0x00, 0x00,  // Authority = 0
      0x00, 0x01,  // Additional = 1
      // Record 1
      0x07, 'r', 'e', 'c', 'o', 'r', 'd', '1',
      0x00,
      0x00, 0x0c,              // TYPE = PTR (12)
      0x00, 0x01,              // CLASS = IN (1)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x0f,              // RDLENGTH = 15 bytes
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      // Record 2
      0x07, 'r', 'e', 'c', 'o', 'r', 'd', '2',
      0x00,
      0x00, 0x01,              // TYPE = A (1)
      0x00, 0x01,              // CLASS = IN (1)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x04,              // RDLENGTH = 4 bytes
      0xac, 0x00, 0x00, 0x01,  // 172.0.0.1
  };
  // clang-format on
  MdnsReader reader(kTestMessage, sizeof(kTestMessage));
  MdnsMessage message;
  EXPECT_TRUE(reader.ReadMdnsMessage(&message));
  EXPECT_EQ(reader.remaining(), UINT64_C(0));

  EXPECT_EQ(message.id(), UINT16_C(1));
  EXPECT_EQ(message.flags(), UINT16_C(0x8400));

  EXPECT_EQ(message.questions().size(), UINT64_C(0));

  ASSERT_EQ(message.answers().size(), UINT64_C(1));
  const MdnsRecord& answer = message.answers().at(0);
  EXPECT_EQ(answer.name().ToString(), "record1");
  EXPECT_EQ(answer.type(), kTypePTR);
  EXPECT_EQ(answer.record_class(), kClassIN);
  EXPECT_EQ(answer.ttl(), UINT32_C(120));
  PtrRecordRdata ptr_rdata(DomainName{"testing", "local"});
  EXPECT_EQ(answer.rdata(), Rdata(ptr_rdata));

  EXPECT_EQ(message.authority_records().size(), UINT64_C(0));

  ASSERT_EQ(message.additional_records().size(), UINT64_C(1));
  const MdnsRecord& additional = message.additional_records().at(0);
  EXPECT_EQ(additional.name().ToString(), "record2");
  EXPECT_EQ(additional.type(), kTypeA);
  EXPECT_EQ(additional.record_class(), kClassIN);
  EXPECT_EQ(additional.ttl(), UINT32_C(120));
  ARecordRdata a_rdata(IPAddress{172, 0, 0, 1});
  EXPECT_EQ(additional.rdata(), Rdata(a_rdata));
}

TEST(MdnsMessageTest, FailToReadInvalidRecordCounts) {
  // clang-format off
  const uint8_t kInvalidMessage1[] = {
      0x00, 0x00,  // ID = 0
      0x00, 0x00,  // FLAGS = 0
      0x00, 0x01,  // Questions = 1
      0x00, 0x01,  // Answers = 1
      0x00, 0x00,  // Authority = 0
      0x00, 0x00,  // Additional = 0
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x0c,  // TYPE = PTR (12)
      0x00, 0x01,  // CLASS = IN (1)
      // NOTE: Missing answer record
  };
  const uint8_t kInvalidMessage2[] = {
      0x00, 0x00,  // ID = 0
      0x00, 0x00,  // FLAGS = 0
      0x00, 0x00,  // Questions = 0
      0x00, 0x00,  // Answers = 0
      0x00, 0x00,  // Authority = 0
      0x00, 0x02,  // Additional = 2
      0x07, 't', 'e', 's', 't', 'i', 'n', 'g',
      0x05, 'l', 'o', 'c', 'a', 'l',
      0x00,
      0x00, 0x0c,              // TYPE = PTR (12)
      0x00, 0x01,              // CLASS = IN (1)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x00,              // RDLENGTH = 0
      // NOTE: Only 1 answer record is given.
  };
  // clang-format on
  MdnsMessage message;
  MdnsReader reader1(kInvalidMessage1, sizeof(kInvalidMessage1));
  EXPECT_FALSE(reader1.ReadMdnsMessage(&message));
  MdnsReader reader2(kInvalidMessage2, sizeof(kInvalidMessage2));
  EXPECT_FALSE(reader2.ReadMdnsMessage(&message));
}

TEST(MdnsMessageTest, Write) {
  // clang-format off
  const uint8_t kExpectedMessage[] = {
      0x00, 0x01,  // ID = 1
      0x04, 0x00,  // FLAGS = AA
      0x00, 0x01,  // Question count
      0x00, 0x00,  // Answer count
      0x00, 0x01,  // Authority count
      0x00, 0x00,  // Additional count
      // Question
      0x08, 'q', 'u', 'e', 's', 't', 'i', 'o', 'n',
      0x00,
      0x00, 0x0c,  // TYPE = PTR (12)
      0x00, 0x01,  // CLASS = IN (1)
      // Authority Record
      0x04, 'a', 'u', 't', 'h',
      0x00,
      0x00, 0x10,              // TYPE = TXT (16)
      0x00, 0x01,              // CLASS = IN (1)
      0x00, 0x00, 0x00, 0x78,  // TTL = 120 seconds
      0x00, 0x0c,              // RDLENGTH = 12 bytes
      0x05, 'f', 'o', 'o', '=', '1',
      0x05, 'b', 'a', 'r', '=', '2',
  };
  // clang-format on
  MdnsQuestion question(DomainName{"question"}, kTypePTR, kClassIN);

  MdnsRecord auth_record(DomainName{"auth"}, kTypeTXT, kClassIN, 120,
                         TxtRecordRdata{"foo=1", "bar=2"});

  MdnsMessage message(1, 0x0400);
  message.AddQuestion(question);
  message.AddAuthorityRecord(auth_record);

  std::vector<uint8_t> buffer(sizeof(kExpectedMessage));
  MdnsWriter writer(buffer.data(), buffer.size());
  EXPECT_TRUE(writer.Write(message));
  EXPECT_EQ(writer.remaining(), UINT64_C(0));
  EXPECT_THAT(buffer, testing::ElementsAreArray(kExpectedMessage));
}

}  // namespace mdns
}  // namespace cast
