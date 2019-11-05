// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/conversion_layer.h"

#include <chrono>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace discovery {
namespace {

static const IPAddress v4_address =
    IPAddress(std::array<uint8_t, 4>{{192, 168, 0, 0}});
static const IPAddress v6_address = IPAddress(std::array<uint8_t, 16>{
    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}});
static const std::string instance_name = "instance";
static const std::string service_name = "_srv-name._udp";
static const std::string domain_name = "local";
static const InstanceKey key = {instance_name, service_name, domain_name};
static constexpr uint16_t port_num = uint16_t{80};

cast::mdns::MdnsRecord CreateFullyPopulatedRecord(uint16_t port = port_num) {
  cast::mdns::DomainName target{instance_name, "_srv-name", "_udp",
                                domain_name};
  auto type = cast::mdns::DnsType::kSRV;
  auto clazz = cast::mdns::DnsClass::kIN;
  auto record_type = cast::mdns::RecordType::kShared;
  auto ttl = std::chrono::seconds(0);
  cast::mdns::SrvRecordRdata srv(0, 0, port, target);
  return cast::mdns::MdnsRecord(target, type, clazz, record_type, ttl, srv);
}

}  // namespace

// TXT Conversions.
TEST(DnsSdConversionLayerTest, TestCreateTxtEmpty) {
  cast::mdns::TxtRecordRdata txt{};
  ErrorOr<DnsSdTxtRecord> record = CreateFromDnsTxt(txt);
  EXPECT_TRUE(record.is_value());
  EXPECT_TRUE(record.value().IsEmpty());
}

TEST(DnsSdConversionLayerTest, TestCreateTxtOnlyEmptyString) {
  cast::mdns::TxtRecordRdata txt;
  ErrorOr<DnsSdTxtRecord> record = CreateFromDnsTxt(txt);
  EXPECT_TRUE(record.is_value());
  EXPECT_TRUE(record.value().IsEmpty());
}

TEST(DnsSdConversionLayerTest, TestCreateTxtValidKeyValue) {
  cast::mdns::TxtRecordRdata txt{"name=value"};
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
  cast::mdns::TxtRecordRdata txt{"=value"};
  ErrorOr<DnsSdTxtRecord> record = CreateFromDnsTxt(txt);
  EXPECT_TRUE(record.is_error());
}

TEST(DnsSdConversionLayerTest, TestCreateTxtValidBool) {
  cast::mdns::TxtRecordRdata txt{"name"};
  ErrorOr<DnsSdTxtRecord> record = CreateFromDnsTxt(txt);
  ASSERT_TRUE(record.is_value());
  ASSERT_TRUE(record.value().GetFlag("name").is_value());
  EXPECT_TRUE(record.value().GetFlag("name").value());
}

// Get*Key functions.
TEST(DnsSdConversionLayerTest, GetSrvKeyFromRecordTest) {
  cast::mdns::MdnsRecord record = CreateFullyPopulatedRecord();
  ErrorOr<InstanceKey> key = GetInstanceKey(record);
  ASSERT_TRUE(key.is_value());
  EXPECT_EQ(key.value().instance_id, instance_name);
  EXPECT_EQ(key.value().service_id, service_name);
  EXPECT_EQ(key.value().domain_id, domain_name);
}

TEST(DnsSdConversionLayerTest, GetPtrKeyFromRecordTest) {
  cast::mdns::MdnsRecord record = CreateFullyPopulatedRecord();
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
  EXPECT_EQ(query.dns_type, cast::mdns::DnsType::kANY);
  EXPECT_EQ(query.dns_class, cast::mdns::DnsClass::kANY);
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
  EXPECT_EQ(query.dns_type, cast::mdns::DnsType::kPTR);
  EXPECT_EQ(query.dns_class, cast::mdns::DnsClass::kANY);
  ASSERT_EQ(query.name.labels().size(), size_t{6});
  EXPECT_EQ(query.name.labels()[0], "_service-id");
  EXPECT_EQ(query.name.labels()[1], "_udp");
  EXPECT_EQ(query.name.labels()[2], "192");
  EXPECT_EQ(query.name.labels()[3], "168");
  EXPECT_EQ(query.name.labels()[4], "0");
  EXPECT_EQ(query.name.labels()[5], "0");
}

}  // namespace discovery
}  // namespace openscreen
