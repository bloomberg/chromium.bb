// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/conversion_layer.h"

#include <algorithm>
#include <chrono>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace discovery {
namespace {

static const IPAddress v4_address =
    IPAddress(std::array<uint8_t, 4>{{192, 168, 0, 0}});
constexpr uint16_t port{1234};
static const IPEndpoint v4_endpoint{v4_address, port};
static const IPAddress v6_address = IPAddress(std::array<uint8_t, 16>{
    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}});
static const IPEndpoint v6_endpoint{v6_address, port};
static const std::string instance_name = "instance";
static const std::string service_part = "_srv-name";
static const std::string protocol_part = "_udp";
static const std::string service_name = "_srv-name._udp";
static const std::string domain_name = "local";
static const InstanceKey key = {instance_name, service_name, domain_name};
static constexpr uint16_t port_num = uint16_t{80};

MdnsRecord CreateFullyPopulatedRecord(uint16_t port = port_num) {
  DomainName target{instance_name, "_srv-name", "_udp", domain_name};
  auto type = DnsType::kSRV;
  auto clazz = DnsClass::kIN;
  auto record_type = RecordType::kShared;
  auto ttl = std::chrono::seconds(0);
  SrvRecordRdata srv(0, 0, port, target);
  return MdnsRecord(target, type, clazz, record_type, ttl, srv);
}

}  // namespace

// TXT Conversions.
TEST(DnsSdConversionLayerTest, TestCreateTxtEmpty) {
  TxtRecordRdata txt{};
  ErrorOr<DnsSdTxtRecord> record = CreateFromDnsTxt(txt);
  EXPECT_TRUE(record.is_value());
  EXPECT_TRUE(record.value().IsEmpty());
}

TEST(DnsSdConversionLayerTest, TestCreateTxtOnlyEmptyString) {
  TxtRecordRdata txt;
  ErrorOr<DnsSdTxtRecord> record = CreateFromDnsTxt(txt);
  EXPECT_TRUE(record.is_value());
  EXPECT_TRUE(record.value().IsEmpty());
}

TEST(DnsSdConversionLayerTest, TestCreateTxtValidKeyValue) {
  TxtRecordRdata txt{"name=value"};
  ErrorOr<DnsSdTxtRecord> record = CreateFromDnsTxt(txt);
  ASSERT_TRUE(record.is_value());
  ASSERT_TRUE(record.value().GetValue("name").is_value());

  // EXPECT_STREQ is causing memory leaks
  std::string expected = "value";
  ASSERT_EQ(record.value().GetValue("name").value().size(), expected.size());
  for (size_t i = 0; i < expected.size(); i++) {
    EXPECT_EQ(expected[i], record.value().GetValue("name").value()[i]);
  }
}

TEST(DnsSdConversionLayerTest, TestCreateTxtInvalidKeyValue) {
  TxtRecordRdata txt{"=value"};
  ErrorOr<DnsSdTxtRecord> record = CreateFromDnsTxt(txt);
  EXPECT_TRUE(record.is_error());
}

TEST(DnsSdConversionLayerTest, TestCreateTxtValidBool) {
  TxtRecordRdata txt{"name"};
  ErrorOr<DnsSdTxtRecord> record = CreateFromDnsTxt(txt);
  ASSERT_TRUE(record.is_value());
  ASSERT_TRUE(record.value().GetFlag("name").is_value());
  EXPECT_TRUE(record.value().GetFlag("name").value());
}

// Get*Key functions.
TEST(DnsSdConversionLayerTest, GetSrvKeyFromRecordTest) {
  MdnsRecord record = CreateFullyPopulatedRecord();
  ErrorOr<InstanceKey> key = GetInstanceKey(record);
  ASSERT_TRUE(key.is_value());
  EXPECT_EQ(key.value().instance_id, instance_name);
  EXPECT_EQ(key.value().service_id, service_name);
  EXPECT_EQ(key.value().domain_id, domain_name);
}

TEST(DnsSdConversionLayerTest, GetPtrKeyFromRecordTest) {
  MdnsRecord record = CreateFullyPopulatedRecord();
  ErrorOr<ServiceKey> key = GetServiceKey(record);
  ASSERT_TRUE(key.is_value());
  EXPECT_EQ(key.value().service_id, service_name);
  EXPECT_EQ(key.value().domain_id, domain_name);
}

TEST(DnsSdConversionLayerTest, GetPtrKeyFromStringsTest) {
  ServiceKey key = GetServiceKey(service_name, domain_name);
  EXPECT_EQ(key.service_id, service_name);
  EXPECT_EQ(key.domain_id, domain_name);
}

// Get*QueryInfo methods.
TEST(DnsSdConversionLayerTest, GetInstanceQueryInfoTest) {
  InstanceKey key{"instance.Id", "_service-id._udp", "192.168.0.0"};
  DnsQueryInfo query = GetInstanceQueryInfo(key);
  EXPECT_EQ(query.dns_type, DnsType::kANY);
  EXPECT_EQ(query.dns_class, DnsClass::kANY);
  ASSERT_EQ(query.name.labels().size(), size_t{7});
  EXPECT_EQ(query.name.labels()[0], "instance.Id");
  EXPECT_EQ(query.name.labels()[1], "_service-id");
  EXPECT_EQ(query.name.labels()[2], "_udp");
  EXPECT_EQ(query.name.labels()[3], "192");
  EXPECT_EQ(query.name.labels()[4], "168");
  EXPECT_EQ(query.name.labels()[5], "0");
  EXPECT_EQ(query.name.labels()[6], "0");
}

TEST(DnsSdConversionLayerTest, GetPtrQueryInfoTest) {
  ServiceKey key{"_service-id._udp", "192.168.0.0"};
  DnsQueryInfo query = GetPtrQueryInfo(key);
  EXPECT_EQ(query.dns_type, DnsType::kPTR);
  EXPECT_EQ(query.dns_class, DnsClass::kANY);
  ASSERT_EQ(query.name.labels().size(), size_t{6});
  EXPECT_EQ(query.name.labels()[0], "_service-id");
  EXPECT_EQ(query.name.labels()[1], "_udp");
  EXPECT_EQ(query.name.labels()[2], "192");
  EXPECT_EQ(query.name.labels()[3], "168");
  EXPECT_EQ(query.name.labels()[4], "0");
  EXPECT_EQ(query.name.labels()[5], "0");
}

// GetDnsRecords tests.
TEST(DnsSdConversionLayerTest, GetDnsRecordsPtr) {
  DnsSdTxtRecord txt;
  DnsSdInstanceRecord instance_record(instance_name, service_name, domain_name,
                                      v4_endpoint, v6_endpoint, txt);
  std::vector<MdnsRecord> records = GetDnsRecords(instance_record);
  auto it = std::find_if(records.begin(), records.end(),
                         [](const MdnsRecord& record) {
                           return record.dns_type() == DnsType::kPTR;
                         });
  ASSERT_NE(it, records.end());

  EXPECT_EQ(it->dns_class(), DnsClass::kIN);
  EXPECT_EQ(it->record_type(), RecordType::kShared);
  EXPECT_EQ(it->name().labels().size(), size_t{3});
  EXPECT_EQ(it->name().labels()[0], service_part);
  EXPECT_EQ(it->name().labels()[1], protocol_part);
  EXPECT_EQ(it->name().labels()[2], domain_name);

  const auto& rdata = absl::get<PtrRecordRdata>(it->rdata());
  EXPECT_EQ(rdata.ptr_domain().labels().size(), size_t{4});
  EXPECT_EQ(rdata.ptr_domain().labels()[0], instance_name);
  EXPECT_EQ(rdata.ptr_domain().labels()[1], service_part);
  EXPECT_EQ(rdata.ptr_domain().labels()[2], protocol_part);
  EXPECT_EQ(rdata.ptr_domain().labels()[3], domain_name);
}

TEST(DnsSdConversionLayerTest, GetDnsRecordsSrv) {
  DnsSdTxtRecord txt;
  DnsSdInstanceRecord instance_record(instance_name, service_name, domain_name,
                                      v4_endpoint, v6_endpoint, txt);
  std::vector<MdnsRecord> records = GetDnsRecords(instance_record);
  auto it = std::find_if(records.begin(), records.end(),
                         [](const MdnsRecord& record) {
                           return record.dns_type() == DnsType::kSRV;
                         });
  ASSERT_NE(it, records.end());

  EXPECT_EQ(it->dns_class(), DnsClass::kIN);
  EXPECT_EQ(it->record_type(), RecordType::kUnique);
  EXPECT_EQ(it->name().labels().size(), size_t{4});
  EXPECT_EQ(it->name().labels()[0], instance_name);
  EXPECT_EQ(it->name().labels()[1], service_part);
  EXPECT_EQ(it->name().labels()[2], protocol_part);
  EXPECT_EQ(it->name().labels()[3], domain_name);

  const auto& rdata = absl::get<SrvRecordRdata>(it->rdata());
  EXPECT_EQ(rdata.priority(), 0);
  EXPECT_EQ(rdata.weight(), 0);
  EXPECT_EQ(rdata.port(), port);
}

TEST(DnsSdConversionLayerTest, GetDnsRecordsAPresent) {
  DnsSdTxtRecord txt;
  DnsSdInstanceRecord instance_record(instance_name, service_name, domain_name,
                                      v4_endpoint, v6_endpoint, txt);
  std::vector<MdnsRecord> records = GetDnsRecords(instance_record);
  auto it = std::find_if(records.begin(), records.end(),
                         [](const MdnsRecord& record) {
                           return record.dns_type() == DnsType::kA;
                         });
  ASSERT_NE(it, records.end());

  EXPECT_EQ(it->dns_class(), DnsClass::kIN);
  EXPECT_EQ(it->record_type(), RecordType::kUnique);
  EXPECT_EQ(it->name().labels().size(), size_t{4});
  EXPECT_EQ(it->name().labels()[0], instance_name);
  EXPECT_EQ(it->name().labels()[1], service_part);
  EXPECT_EQ(it->name().labels()[2], protocol_part);
  EXPECT_EQ(it->name().labels()[3], domain_name);

  const auto& rdata = absl::get<ARecordRdata>(it->rdata());
  EXPECT_EQ(rdata.ipv4_address(), v4_address);
}

TEST(DnsSdConversionLayerTest, GetDnsRecordsANotPresent) {
  DnsSdTxtRecord txt;
  DnsSdInstanceRecord instance_record(instance_name, service_name, domain_name,
                                      v6_endpoint, txt);
  std::vector<MdnsRecord> records = GetDnsRecords(instance_record);
  auto it = std::find_if(records.begin(), records.end(),
                         [](const MdnsRecord& record) {
                           return record.dns_type() == DnsType::kA;
                         });
  EXPECT_EQ(it, records.end());
}

TEST(DnsSdConversionLayerTest, GetDnsRecordsAAAAPresent) {
  DnsSdTxtRecord txt;
  DnsSdInstanceRecord instance_record(instance_name, service_name, domain_name,
                                      v4_endpoint, v6_endpoint, txt);
  std::vector<MdnsRecord> records = GetDnsRecords(instance_record);
  auto it = std::find_if(records.begin(), records.end(),
                         [](const MdnsRecord& record) {
                           return record.dns_type() == DnsType::kAAAA;
                         });
  ASSERT_NE(it, records.end());

  EXPECT_EQ(it->dns_class(), DnsClass::kIN);
  EXPECT_EQ(it->record_type(), RecordType::kUnique);
  EXPECT_EQ(it->name().labels().size(), size_t{4});
  EXPECT_EQ(it->name().labels()[0], instance_name);
  EXPECT_EQ(it->name().labels()[1], service_part);
  EXPECT_EQ(it->name().labels()[2], protocol_part);
  EXPECT_EQ(it->name().labels()[3], domain_name);

  const auto& rdata = absl::get<AAAARecordRdata>(it->rdata());
  EXPECT_EQ(rdata.ipv6_address(), v6_address);
}

TEST(DnsSdConversionLayerTest, GetDnsRecordsAAAANotPresent) {
  DnsSdTxtRecord txt;
  DnsSdInstanceRecord instance_record(instance_name, service_name, domain_name,
                                      v4_endpoint, txt);
  std::vector<MdnsRecord> records = GetDnsRecords(instance_record);
  auto it = std::find_if(records.begin(), records.end(),
                         [](const MdnsRecord& record) {
                           return record.dns_type() == DnsType::kAAAA;
                         });
  EXPECT_EQ(it, records.end());
}

TEST(DnsSdConversionLayerTest, GetDnsRecordsTxt) {
  DnsSdTxtRecord txt;
  auto value =
      absl::Span<const uint8_t>(reinterpret_cast<const uint8_t*>("value"), 5);
  txt.SetValue("name", value);
  txt.SetFlag("boolean", true);
  DnsSdInstanceRecord instance_record(instance_name, service_name, domain_name,
                                      v4_endpoint, v6_endpoint, txt);
  std::vector<MdnsRecord> records = GetDnsRecords(instance_record);
  auto it = std::find_if(records.begin(), records.end(),
                         [](const MdnsRecord& record) {
                           return record.dns_type() == DnsType::kTXT;
                         });
  ASSERT_NE(it, records.end());

  EXPECT_EQ(it->dns_class(), DnsClass::kIN);
  EXPECT_EQ(it->record_type(), RecordType::kUnique);
  EXPECT_EQ(it->name().labels().size(), size_t{4});
  EXPECT_EQ(it->name().labels()[0], instance_name);
  EXPECT_EQ(it->name().labels()[1], service_part);
  EXPECT_EQ(it->name().labels()[2], protocol_part);
  EXPECT_EQ(it->name().labels()[3], domain_name);

  const auto& rdata = absl::get<TxtRecordRdata>(it->rdata());
  EXPECT_EQ(rdata.texts().size(), size_t{2});

  auto it2 =
      std::find(rdata.texts().begin(), rdata.texts().end(), "name=value");
  EXPECT_NE(it2, rdata.texts().end());

  it2 = std::find(rdata.texts().begin(), rdata.texts().end(), "boolean");
  EXPECT_NE(it2, rdata.texts().end());
}

}  // namespace discovery
}  // namespace openscreen
