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

namespace {

constexpr std::chrono::seconds kTtl{120};

template <class T>
void TestCopyAndMove(const T& value) {
  T value_copy_constuct(value);
  EXPECT_EQ(value_copy_constuct, value);
  T value_copy_assign = value;
  EXPECT_EQ(value_copy_assign, value);
  T value_move_constuct(std::move(value_copy_constuct));
  EXPECT_EQ(value_move_constuct, value);
  T value_move_assign = std::move(value_copy_assign);
  EXPECT_EQ(value_move_assign, value);
}

}  // namespace

TEST(MdnsDomainNameTest, Construct) {
  DomainName name1;
  EXPECT_TRUE(name1.empty());
  EXPECT_EQ(name1.MaxWireSize(), UINT64_C(1));
  EXPECT_EQ(name1.labels().size(), UINT64_C(0));

  DomainName name2{"MyDevice", "_mYSERvice", "local"};
  EXPECT_FALSE(name2.empty());
  EXPECT_EQ(name2.MaxWireSize(), UINT64_C(27));
  ASSERT_EQ(name2.labels().size(), UINT64_C(3));
  EXPECT_EQ(name2.labels()[0], "MyDevice");
  EXPECT_EQ(name2.labels()[1], "_mYSERvice");
  EXPECT_EQ(name2.labels()[2], "local");
  EXPECT_EQ(name2.ToString(), "MyDevice._mYSERvice.local");

  std::vector<absl::string_view> labels{"OtherDevice", "_MYservice", "LOcal"};
  DomainName name3(labels);
  EXPECT_FALSE(name3.empty());
  EXPECT_EQ(name3.MaxWireSize(), UINT64_C(30));
  ASSERT_EQ(name3.labels().size(), UINT64_C(3));
  EXPECT_EQ(name3.labels()[0], "OtherDevice");
  EXPECT_EQ(name3.labels()[1], "_MYservice");
  EXPECT_EQ(name3.labels()[2], "LOcal");
  EXPECT_EQ(name3.ToString(), "OtherDevice._MYservice.LOcal");
}

TEST(MdnsDomainNameTest, Compare) {
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

TEST(MdnsDomainNameTest, CopyAndMove) {
  TestCopyAndMove(DomainName{"testing", "local"});
}

TEST(MdnsRawRecordRdataTest, Construct) {
  constexpr uint8_t kRawRdata[] = {
      0x05, 'c', 'n', 'a', 'm', 'e', 0xc0, 0x00,
  };

  RawRecordRdata rdata1;
  EXPECT_EQ(rdata1.MaxWireSize(), UINT64_C(2));
  EXPECT_EQ(rdata1.size(), UINT16_C(0));

  RawRecordRdata rdata2(kRawRdata, sizeof(kRawRdata));
  EXPECT_EQ(rdata2.MaxWireSize(), UINT64_C(10));
  EXPECT_EQ(rdata2.size(), UINT16_C(8));
  EXPECT_THAT(
      std::vector<uint8_t>(rdata2.data(), rdata2.data() + rdata2.size()),
      testing::ElementsAreArray(kRawRdata));

  RawRecordRdata rdata3(
      std::vector<uint8_t>(kRawRdata, kRawRdata + sizeof(kRawRdata)));
  EXPECT_EQ(rdata3.MaxWireSize(), UINT64_C(10));
  EXPECT_EQ(rdata3.size(), UINT16_C(8));
  EXPECT_THAT(
      std::vector<uint8_t>(rdata3.data(), rdata3.data() + rdata3.size()),
      testing::ElementsAreArray(kRawRdata));
}

TEST(MdnsRawRecordRdataTest, Compare) {
  constexpr uint8_t kRawRdata1[] = {
      0x05, 'c', 'n', 'a', 'm', 'e', 0xc0, 0x00,
  };
  constexpr uint8_t kRawRdata2[] = {
      0x05, 'r', 'd', 'a', 't', 'a',
  };

  RawRecordRdata rdata1(kRawRdata1, sizeof(kRawRdata1));
  RawRecordRdata rdata2(kRawRdata1, sizeof(kRawRdata1));
  RawRecordRdata rdata3(kRawRdata2, sizeof(kRawRdata2));

  EXPECT_EQ(rdata1, rdata2);
  EXPECT_NE(rdata1, rdata3);
}

TEST(MdnsRawRecordRdataTest, CopyAndMove) {
  constexpr uint8_t kRawRdata[] = {
      0x05, 'c', 'n', 'a', 'm', 'e', 0xc0, 0x00,
  };
  TestCopyAndMove(RawRecordRdata(kRawRdata, sizeof(kRawRdata)));
}

TEST(MdnsSrvRecordRdataTest, Construct) {
  SrvRecordRdata rdata1;
  EXPECT_EQ(rdata1.MaxWireSize(), UINT64_C(9));
  EXPECT_EQ(rdata1.priority(), UINT16_C(0));
  EXPECT_EQ(rdata1.weight(), UINT16_C(0));
  EXPECT_EQ(rdata1.port(), UINT16_C(0));
  EXPECT_EQ(rdata1.target(), DomainName());

  SrvRecordRdata rdata2(1, 2, 3, DomainName{"testing", "local"});
  EXPECT_EQ(rdata2.MaxWireSize(), UINT64_C(23));
  EXPECT_EQ(rdata2.priority(), UINT16_C(1));
  EXPECT_EQ(rdata2.weight(), UINT16_C(2));
  EXPECT_EQ(rdata2.port(), UINT16_C(3));
  EXPECT_EQ(rdata2.target(), (DomainName{"testing", "local"}));
}

TEST(MdnsSrvRecordRdataTest, Compare) {
  SrvRecordRdata rdata1(1, 2, 3, DomainName{"testing", "local"});
  SrvRecordRdata rdata2(1, 2, 3, DomainName{"testing", "local"});
  SrvRecordRdata rdata3(4, 2, 3, DomainName{"testing", "local"});
  SrvRecordRdata rdata4(1, 5, 3, DomainName{"testing", "local"});
  SrvRecordRdata rdata5(1, 2, 6, DomainName{"testing", "local"});
  SrvRecordRdata rdata6(1, 2, 3, DomainName{"device", "local"});

  EXPECT_EQ(rdata1, rdata2);
  EXPECT_NE(rdata1, rdata3);
  EXPECT_NE(rdata1, rdata4);
  EXPECT_NE(rdata1, rdata5);
  EXPECT_NE(rdata1, rdata6);
}

TEST(MdnsSrvRecordRdataTest, CopyAndMove) {
  TestCopyAndMove(SrvRecordRdata(1, 2, 3, DomainName{"testing", "local"}));
}

TEST(MdnsARecordRdataTest, Construct) {
  ARecordRdata rdata1;
  EXPECT_EQ(rdata1.MaxWireSize(), UINT64_C(6));
  EXPECT_EQ(rdata1.ipv4_address(), (IPAddress{0, 0, 0, 0}));

  ARecordRdata rdata2(IPAddress{8, 8, 8, 8});
  EXPECT_EQ(rdata2.MaxWireSize(), UINT64_C(6));
  EXPECT_EQ(rdata2.ipv4_address(), (IPAddress{8, 8, 8, 8}));
}

TEST(MdnsARecordRdataTest, Compare) {
  ARecordRdata rdata1(IPAddress{8, 8, 8, 8});
  ARecordRdata rdata2(IPAddress{8, 8, 8, 8});
  ARecordRdata rdata3(IPAddress{1, 2, 3, 4});

  EXPECT_EQ(rdata1, rdata2);
  EXPECT_NE(rdata1, rdata3);
}

TEST(MdnsARecordRdataTest, CopyAndMove) {
  TestCopyAndMove(ARecordRdata(IPAddress{8, 8, 8, 8}));
}

TEST(MdnsAAAARecordRdataTest, Construct) {
  constexpr uint8_t kIPv6AddressBytes1[] = {
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  constexpr uint8_t kIPv6AddressBytes2[] = {
      0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x02, 0x02, 0xb3, 0xff, 0xfe, 0x1e, 0x83, 0x29,
  };

  IPAddress address1(kIPv6AddressBytes1);
  AAAARecordRdata rdata1;
  EXPECT_EQ(rdata1.MaxWireSize(), UINT64_C(18));
  EXPECT_EQ(rdata1.ipv6_address(), address1);

  IPAddress address2(kIPv6AddressBytes2);
  AAAARecordRdata rdata2(address2);
  EXPECT_EQ(rdata2.MaxWireSize(), UINT64_C(18));
  EXPECT_EQ(rdata2.ipv6_address(), address2);
}

TEST(MdnsAAAARecordRdataTest, Compare) {
  constexpr uint8_t kIPv6AddressBytes1[] = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
  };
  constexpr uint8_t kIPv6AddressBytes2[] = {
      0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x02, 0x02, 0xb3, 0xff, 0xfe, 0x1e, 0x83, 0x29,
  };

  IPAddress address1(kIPv6AddressBytes1);
  IPAddress address2(kIPv6AddressBytes2);
  AAAARecordRdata rdata1(address1);
  AAAARecordRdata rdata2(address1);
  AAAARecordRdata rdata3(address2);

  EXPECT_EQ(rdata1, rdata2);
  EXPECT_NE(rdata1, rdata3);
}

TEST(MdnsAAAARecordRdataTest, CopyAndMove) {
  constexpr uint8_t kIPv6AddressBytes[] = {
      0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x02, 0x02, 0xb3, 0xff, 0xfe, 0x1e, 0x83, 0x29,
  };
  TestCopyAndMove(AAAARecordRdata(IPAddress(kIPv6AddressBytes)));
}

TEST(MdnsPtrRecordRdataTest, Construct) {
  PtrRecordRdata rdata1;
  EXPECT_EQ(rdata1.MaxWireSize(), UINT64_C(3));
  EXPECT_EQ(rdata1.ptr_domain(), DomainName());

  PtrRecordRdata rdata2(DomainName{"testing", "local"});
  EXPECT_EQ(rdata2.MaxWireSize(), UINT64_C(17));
  EXPECT_EQ(rdata2.ptr_domain(), (DomainName{"testing", "local"}));
}

TEST(MdnsPtrRecordRdataTest, Compare) {
  PtrRecordRdata rdata1(DomainName{"testing", "local"});
  PtrRecordRdata rdata2(DomainName{"testing", "local"});
  PtrRecordRdata rdata3(DomainName{"device", "local"});

  EXPECT_EQ(rdata1, rdata2);
  EXPECT_NE(rdata1, rdata3);
}

TEST(MdnsPtrRecordRdataTest, CopyAndMove) {
  TestCopyAndMove(PtrRecordRdata(DomainName{"testing", "local"}));
}

TEST(MdnsTxtRecordRdataTest, Construct) {
  TxtRecordRdata rdata1;
  EXPECT_EQ(rdata1.MaxWireSize(), UINT64_C(3));
  EXPECT_EQ(rdata1.texts(), std::vector<std::string>());

  TxtRecordRdata rdata2{"foo=1", "bar=2"};
  EXPECT_EQ(rdata2.MaxWireSize(), UINT64_C(14));
  EXPECT_EQ(rdata2.texts(), (std::vector<std::string>{"foo=1", "bar=2"}));

  std::vector<absl::string_view> texts{"E=mc^2", "F=ma", "P=IV"};
  TxtRecordRdata rdata3(texts);
  EXPECT_EQ(rdata3.MaxWireSize(), UINT64_C(19));
  EXPECT_EQ(rdata3.texts(),
            (std::vector<std::string>{"E=mc^2", "F=ma", "P=IV"}));
}

TEST(MdnsTxtRecordRdataTest, Compare) {
  TxtRecordRdata rdata1{"foo=1", "bar=2"};
  TxtRecordRdata rdata2{"foo=1", "bar=2"};
  TxtRecordRdata rdata3{"foo=1"};
  TxtRecordRdata rdata4{"E=mc^2", "F=ma"};

  EXPECT_EQ(rdata1, rdata2);
  EXPECT_NE(rdata1, rdata3);
  EXPECT_NE(rdata1, rdata4);
}

TEST(MdnsTxtRecordRdataTest, CopyAndMove) {
  TestCopyAndMove(TxtRecordRdata{"foo=1", "bar=2"});
}

TEST(MdnsRecordTest, Construct) {
  MdnsRecord record1;
  EXPECT_EQ(record1.MaxWireSize(), UINT64_C(11));
  EXPECT_EQ(record1.name(), DomainName());
  EXPECT_EQ(record1.dns_type(), static_cast<DnsType>(0));
  EXPECT_EQ(record1.dns_class(), static_cast<DnsClass>(0));
  EXPECT_EQ(record1.record_type(), RecordType::kShared);
  EXPECT_EQ(record1.ttl(),
            std::chrono::seconds(255));  // 255 is kDefaultRecordTTLSeconds
  EXPECT_EQ(record1.rdata(), Rdata(RawRecordRdata()));

  MdnsRecord record2(DomainName{"hostname", "local"}, DnsType::kPTR,
                     DnsClass::kIN, RecordType::kUnique, kTtl,
                     PtrRecordRdata(DomainName{"testing", "local"}));
  EXPECT_EQ(record2.MaxWireSize(), UINT64_C(41));
  EXPECT_EQ(record2.name(), (DomainName{"hostname", "local"}));
  EXPECT_EQ(record2.dns_type(), DnsType::kPTR);
  EXPECT_EQ(record2.dns_class(), DnsClass::kIN);
  EXPECT_EQ(record2.record_type(), RecordType::kUnique);
  EXPECT_EQ(record2.ttl(), kTtl);
  EXPECT_EQ(record2.rdata(),
            Rdata(PtrRecordRdata(DomainName{"testing", "local"})));
}

TEST(MdnsRecordTest, Compare) {
  MdnsRecord record1(DomainName{"hostname", "local"}, DnsType::kPTR,
                     DnsClass::kIN, RecordType::kShared, kTtl,
                     PtrRecordRdata(DomainName{"testing", "local"}));
  MdnsRecord record2(DomainName{"hostname", "local"}, DnsType::kPTR,
                     DnsClass::kIN, RecordType::kShared, kTtl,
                     PtrRecordRdata(DomainName{"testing", "local"}));
  MdnsRecord record3(DomainName{"othername", "local"}, DnsType::kPTR,
                     DnsClass::kIN, RecordType::kShared, kTtl,
                     PtrRecordRdata(DomainName{"testing", "local"}));
  MdnsRecord record4(DomainName{"hostname", "local"}, DnsType::kA,
                     DnsClass::kIN, RecordType::kShared, kTtl,
                     ARecordRdata(IPAddress{8, 8, 8, 8}));
  MdnsRecord record5(DomainName{"hostname", "local"}, DnsType::kPTR,
                     DnsClass::kIN, RecordType::kUnique, kTtl,
                     PtrRecordRdata(DomainName{"testing", "local"}));
  MdnsRecord record6(DomainName{"hostname", "local"}, DnsType::kPTR,
                     DnsClass::kIN, RecordType::kShared,
                     std::chrono::seconds(200),
                     PtrRecordRdata(DomainName{"testing", "local"}));
  MdnsRecord record7(DomainName{"hostname", "local"}, DnsType::kPTR,
                     DnsClass::kIN, RecordType::kShared, kTtl,
                     PtrRecordRdata(DomainName{"device", "local"}));

  EXPECT_EQ(record1, record2);
  EXPECT_NE(record1, record3);
  EXPECT_NE(record1, record4);
  EXPECT_NE(record1, record5);
  EXPECT_NE(record1, record6);
  EXPECT_NE(record1, record7);
}

TEST(MdnsRecordTest, CopyAndMove) {
  MdnsRecord record(DomainName{"hostname", "local"}, DnsType::kPTR,
                    DnsClass::kIN, RecordType::kUnique, kTtl,
                    PtrRecordRdata(DomainName{"testing", "local"}));
  TestCopyAndMove(record);
}

TEST(MdnsQuestionTest, Construct) {
  MdnsQuestion question1;
  EXPECT_EQ(question1.MaxWireSize(), UINT64_C(5));
  EXPECT_EQ(question1.name(), DomainName());
  EXPECT_EQ(question1.dns_type(), static_cast<DnsType>(0));
  EXPECT_EQ(question1.dns_class(), static_cast<DnsClass>(0));
  EXPECT_EQ(question1.response_type(), ResponseType::kMulticast);

  MdnsQuestion question2(DomainName{"testing", "local"}, DnsType::kPTR,
                         DnsClass::kIN, ResponseType::kUnicast);
  EXPECT_EQ(question2.MaxWireSize(), UINT64_C(19));
  EXPECT_EQ(question2.name(), (DomainName{"testing", "local"}));
  EXPECT_EQ(question2.dns_type(), DnsType::kPTR);
  EXPECT_EQ(question2.dns_class(), DnsClass::kIN);
  EXPECT_EQ(question2.response_type(), ResponseType::kUnicast);
}

TEST(MdnsQuestionTest, Compare) {
  MdnsQuestion question1(DomainName{"testing", "local"}, DnsType::kPTR,
                         DnsClass::kIN, ResponseType::kMulticast);
  MdnsQuestion question2(DomainName{"testing", "local"}, DnsType::kPTR,
                         DnsClass::kIN, ResponseType::kMulticast);
  MdnsQuestion question3(DomainName{"hostname", "local"}, DnsType::kPTR,
                         DnsClass::kIN, ResponseType::kMulticast);
  MdnsQuestion question4(DomainName{"testing", "local"}, DnsType::kA,
                         DnsClass::kIN, ResponseType::kMulticast);
  MdnsQuestion question5(DomainName{"hostname", "local"}, DnsType::kPTR,
                         DnsClass::kIN, ResponseType::kUnicast);

  EXPECT_EQ(question1, question2);
  EXPECT_NE(question1, question3);
  EXPECT_NE(question1, question4);
  EXPECT_NE(question1, question5);
}

TEST(MdnsQuestionTest, CopyAndMove) {
  MdnsQuestion question(DomainName{"testing", "local"}, DnsType::kPTR,
                        DnsClass::kIN, ResponseType::kUnicast);
  TestCopyAndMove(question);
}

TEST(MdnsMessageTest, Construct) {
  MdnsMessage message1;
  EXPECT_EQ(message1.MaxWireSize(), UINT64_C(12));
  EXPECT_EQ(message1.id(), UINT16_C(0));
  EXPECT_EQ(message1.type(), MessageType::Query);
  EXPECT_EQ(message1.questions().size(), UINT64_C(0));
  EXPECT_EQ(message1.answers().size(), UINT64_C(0));
  EXPECT_EQ(message1.authority_records().size(), UINT64_C(0));
  EXPECT_EQ(message1.additional_records().size(), UINT64_C(0));

  MdnsQuestion question(DomainName{"testing", "local"}, DnsType::kPTR,
                        DnsClass::kIN, ResponseType::kUnicast);
  MdnsRecord record1(DomainName{"record1"}, DnsType::kA, DnsClass::kIN,
                     RecordType::kShared, kTtl,
                     ARecordRdata(IPAddress{172, 0, 0, 1}));
  MdnsRecord record2(DomainName{"record2"}, DnsType::kTXT, DnsClass::kIN,
                     RecordType::kShared, kTtl,
                     TxtRecordRdata{"foo=1", "bar=2"});
  MdnsRecord record3(DomainName{"record3"}, DnsType::kPTR, DnsClass::kIN,
                     RecordType::kShared, kTtl,
                     PtrRecordRdata(DomainName{"device", "local"}));

  MdnsMessage message2(123, MessageType::Response);
  EXPECT_EQ(message2.MaxWireSize(), UINT64_C(12));
  EXPECT_EQ(message2.id(), UINT16_C(123));
  EXPECT_EQ(message2.type(), MessageType::Response);
  EXPECT_EQ(message2.questions().size(), UINT64_C(0));
  EXPECT_EQ(message2.answers().size(), UINT64_C(0));
  EXPECT_EQ(message2.authority_records().size(), UINT64_C(0));
  EXPECT_EQ(message2.additional_records().size(), UINT64_C(0));

  message2.AddQuestion(question);
  message2.AddAnswer(record1);
  message2.AddAuthorityRecord(record2);
  message2.AddAdditionalRecord(record3);

  EXPECT_EQ(message2.MaxWireSize(), UINT64_C(118));
  ASSERT_EQ(message2.questions().size(), UINT64_C(1));
  ASSERT_EQ(message2.answers().size(), UINT64_C(1));
  ASSERT_EQ(message2.authority_records().size(), UINT64_C(1));
  ASSERT_EQ(message2.additional_records().size(), UINT64_C(1));

  EXPECT_EQ(message2.questions()[0], question);
  EXPECT_EQ(message2.answers()[0], record1);
  EXPECT_EQ(message2.authority_records()[0], record2);
  EXPECT_EQ(message2.additional_records()[0], record3);

  MdnsMessage message3(
      123, MessageType::Response, std::vector<MdnsQuestion>{question},
      std::vector<MdnsRecord>{record1}, std::vector<MdnsRecord>{record2},
      std::vector<MdnsRecord>{record3});

  EXPECT_EQ(message3.MaxWireSize(), UINT64_C(118));
  ASSERT_EQ(message3.questions().size(), UINT64_C(1));
  ASSERT_EQ(message3.answers().size(), UINT64_C(1));
  ASSERT_EQ(message3.authority_records().size(), UINT64_C(1));
  ASSERT_EQ(message3.additional_records().size(), UINT64_C(1));

  EXPECT_EQ(message3.questions()[0], question);
  EXPECT_EQ(message3.answers()[0], record1);
  EXPECT_EQ(message3.authority_records()[0], record2);
  EXPECT_EQ(message3.additional_records()[0], record3);
}

TEST(MdnsMessageTest, Compare) {
  MdnsQuestion question(DomainName{"testing", "local"}, DnsType::kPTR,
                        DnsClass::kIN, ResponseType::kUnicast);
  MdnsRecord record1(DomainName{"record1"}, DnsType::kA, DnsClass::kIN,
                     RecordType::kShared, kTtl,
                     ARecordRdata(IPAddress{172, 0, 0, 1}));
  MdnsRecord record2(DomainName{"record2"}, DnsType::kTXT, DnsClass::kIN,
                     RecordType::kShared, kTtl,
                     TxtRecordRdata{"foo=1", "bar=2"});
  MdnsRecord record3(DomainName{"record3"}, DnsType::kPTR, DnsClass::kIN,
                     RecordType::kShared, kTtl,
                     PtrRecordRdata(DomainName{"device", "local"}));

  MdnsMessage message1(
      123, MessageType::Response, std::vector<MdnsQuestion>{question},
      std::vector<MdnsRecord>{record1}, std::vector<MdnsRecord>{record2},
      std::vector<MdnsRecord>{record3});
  MdnsMessage message2(
      123, MessageType::Response, std::vector<MdnsQuestion>{question},
      std::vector<MdnsRecord>{record1}, std::vector<MdnsRecord>{record2},
      std::vector<MdnsRecord>{record3});
  MdnsMessage message3(
      456, MessageType::Response, std::vector<MdnsQuestion>{question},
      std::vector<MdnsRecord>{record1}, std::vector<MdnsRecord>{record2},
      std::vector<MdnsRecord>{record3});
  MdnsMessage message4(
      123, MessageType::Query, std::vector<MdnsQuestion>{question},
      std::vector<MdnsRecord>{record1}, std::vector<MdnsRecord>{record2},
      std::vector<MdnsRecord>{record3});
  MdnsMessage message5(123, MessageType::Response, std::vector<MdnsQuestion>{},
                       std::vector<MdnsRecord>{record1},
                       std::vector<MdnsRecord>{record2},
                       std::vector<MdnsRecord>{record3});
  MdnsMessage message6(
      123, MessageType::Response, std::vector<MdnsQuestion>{question},
      std::vector<MdnsRecord>{}, std::vector<MdnsRecord>{record2},
      std::vector<MdnsRecord>{record3});
  MdnsMessage message7(
      123, MessageType::Response, std::vector<MdnsQuestion>{question},
      std::vector<MdnsRecord>{record1}, std::vector<MdnsRecord>{},
      std::vector<MdnsRecord>{record3});
  MdnsMessage message8(
      123, MessageType::Response, std::vector<MdnsQuestion>{question},
      std::vector<MdnsRecord>{record1}, std::vector<MdnsRecord>{record2},
      std::vector<MdnsRecord>{});

  EXPECT_EQ(message1, message2);
  EXPECT_NE(message1, message3);
  EXPECT_NE(message1, message4);
  EXPECT_NE(message1, message5);
  EXPECT_NE(message1, message6);
  EXPECT_NE(message1, message7);
  EXPECT_NE(message1, message8);
}

TEST(MdnsMessageTest, CopyAndMove) {
  MdnsQuestion question(DomainName{"testing", "local"}, DnsType::kPTR,
                        DnsClass::kIN, ResponseType::kUnicast);
  MdnsRecord record1(DomainName{"record1"}, DnsType::kA, DnsClass::kIN,
                     RecordType::kShared, kTtl,
                     ARecordRdata(IPAddress{172, 0, 0, 1}));
  MdnsRecord record2(DomainName{"record2"}, DnsType::kTXT, DnsClass::kIN,
                     RecordType::kShared, kTtl,
                     TxtRecordRdata{"foo=1", "bar=2"});
  MdnsRecord record3(DomainName{"record3"}, DnsType::kPTR, DnsClass::kIN,
                     RecordType::kShared, kTtl,
                     PtrRecordRdata(DomainName{"device", "local"}));
  MdnsMessage message(
      123, MessageType::Response, std::vector<MdnsQuestion>{question},
      std::vector<MdnsRecord>{record1}, std::vector<MdnsRecord>{record2},
      std::vector<MdnsRecord>{record3});
  TestCopyAndMove(message);
}

}  // namespace mdns
}  // namespace cast
