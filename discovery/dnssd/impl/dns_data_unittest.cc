// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/dns_data.h"

#include <chrono>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace discovery {

class DnsDataTesting : public DnsData {
 public:
  explicit DnsDataTesting(const InstanceKey& instance_key)
      : DnsData(instance_key) {}

  void set_srv(absl::optional<cast::mdns::SrvRecordRdata> new_srv) {
    SetVariable(new_srv, srv(), cast::mdns::DnsType::kSRV);
  }

  void set_txt(absl::optional<cast::mdns::TxtRecordRdata> new_txt) {
    SetVariable(new_txt, txt(), cast::mdns::DnsType::kTXT);
  }

  void set_a(absl::optional<cast::mdns::ARecordRdata> new_a) {
    SetVariable(new_a, a(), cast::mdns::DnsType::kA);
  }

  void set_aaaa(absl::optional<cast::mdns::AAAARecordRdata> new_aaaa) {
    SetVariable(new_aaaa, aaaa(), cast::mdns::DnsType::kAAAA);
  }

  const absl::optional<cast::mdns::SrvRecordRdata>& srv() { return srv_; };
  const absl::optional<cast::mdns::TxtRecordRdata>& txt() { return txt_; };
  const absl::optional<cast::mdns::ARecordRdata>& a() { return a_; };
  const absl::optional<cast::mdns::AAAARecordRdata>& aaaa() { return aaaa_; };

 private:
  template <typename T>
  void SetVariable(absl::optional<T> new_val,
                   const absl::optional<T>& old_val,
                   cast::mdns::DnsType type) {
    if (!new_val.has_value() && !old_val.has_value()) {
      return;
    }

    if (!new_val.has_value()) {
      cast::mdns::MdnsRecord record(cast::mdns::DomainName{"0"}, type,
                                    cast::mdns::DnsClass::kIN,
                                    cast::mdns::RecordType::kUnique,
                                    std::chrono::seconds(1), old_val.value());
      ApplyDataRecordChange(record, cast::mdns::RecordChangedEvent::kDeleted);
      return;
    }

    cast::mdns::MdnsRecord record(cast::mdns::DomainName{"0"}, type,
                                  cast::mdns::DnsClass::kIN,
                                  cast::mdns::RecordType::kUnique,
                                  std::chrono::seconds(1), new_val.value());
    if (!old_val.has_value()) {
      ApplyDataRecordChange(record, cast::mdns::RecordChangedEvent::kCreated);
    } else {
      ApplyDataRecordChange(record, cast::mdns::RecordChangedEvent::kUpdated);
    }
  }
};

static const IPAddress v4_address =
    IPAddress(std::array<uint8_t, 4>{{192, 168, 0, 0}});
static const IPAddress v6_address = IPAddress(std::array<uint8_t, 16>{
    {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}});
static const std::string instance_name = "instance";
static const std::string service_name = "_srv-name._udp";
static const std::string domain_name = "local";
static const InstanceKey key = {instance_name, service_name, domain_name};
static constexpr uint16_t port_num = uint16_t{80};

DnsDataTesting CreateFullyPopulatedData() {
  InstanceKey instance{instance_name, service_name, domain_name};
  DnsDataTesting data(instance);
  cast::mdns::DomainName target{instance_name, "_srv-name", "_udp",
                                domain_name};
  cast::mdns::SrvRecordRdata srv(0, 0, port_num, target);
  cast::mdns::TxtRecordRdata txt{"name=value", "boolValue"};
  cast::mdns::ARecordRdata a(v4_address);
  cast::mdns::AAAARecordRdata aaaa(v6_address);

  data.set_srv(srv);
  data.set_txt(txt);
  data.set_a(a);
  data.set_aaaa(aaaa);

  return data;
}

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

// DnsData Conversions.
TEST(DnsSdDnsDataTests, TestConvertDnsDataCorrectly) {
  DnsDataTesting data = CreateFullyPopulatedData();
  ErrorOr<DnsSdInstanceRecord> result = data.CreateRecord();
  ASSERT_TRUE(result.is_value());

  DnsSdInstanceRecord record = result.value();
  ASSERT_TRUE(record.address_v4().has_value());
  ASSERT_TRUE(record.address_v6().has_value());
  EXPECT_EQ(record.instance_id(), instance_name);
  EXPECT_EQ(record.service_id(), service_name);
  EXPECT_EQ(record.domain_id(), domain_name);
  EXPECT_EQ(record.address_v4().value().port, port_num);
  EXPECT_EQ(record.address_v4().value().address, v4_address);
  EXPECT_EQ(record.address_v6().value().port, port_num);
  EXPECT_EQ(record.address_v6().value().address, v6_address);
  EXPECT_FALSE(record.txt().IsEmpty());
}

TEST(DnsSdDnsDataTests, TestConvertDnsDataMissingData) {
  DnsDataTesting data = CreateFullyPopulatedData();
  EXPECT_TRUE(data.CreateRecord().is_value());

  data = CreateFullyPopulatedData();
  data.set_srv(absl::nullopt);
  EXPECT_FALSE(data.CreateRecord().is_value());

  data = CreateFullyPopulatedData();
  data.set_txt(absl::nullopt);
  EXPECT_FALSE(data.CreateRecord().is_value());

  data = CreateFullyPopulatedData();
  data.set_a(absl::nullopt);
  EXPECT_TRUE(data.CreateRecord().is_value());

  data = CreateFullyPopulatedData();
  data.set_aaaa(absl::nullopt);
  EXPECT_TRUE(data.CreateRecord().is_value());

  data = CreateFullyPopulatedData();
  data.set_a(absl::nullopt);
  data.set_aaaa(absl::nullopt);
  EXPECT_FALSE(data.CreateRecord().is_value());
}

TEST(DnsSdDnsDataTests, TestConvertDnsDataOneAddress) {
  // Address v4.
  DnsDataTesting data = CreateFullyPopulatedData();
  data.set_aaaa(absl::nullopt);
  ErrorOr<DnsSdInstanceRecord> result = data.CreateRecord();
  ASSERT_TRUE(result.is_value());

  DnsSdInstanceRecord record = result.value();
  EXPECT_FALSE(record.address_v6().has_value());
  ASSERT_TRUE(record.address_v4().has_value());
  EXPECT_EQ(record.address_v4().value().port, port_num);
  EXPECT_EQ(record.address_v4().value().address, v4_address);

  // Address v6.
  data = CreateFullyPopulatedData();
  data.set_a(absl::nullopt);
  result = data.CreateRecord();
  ASSERT_TRUE(result.is_value());

  record = result.value();
  EXPECT_FALSE(record.address_v4().has_value());
  ASSERT_TRUE(record.address_v6().has_value());
  EXPECT_EQ(record.address_v6().value().port, port_num);
  EXPECT_EQ(record.address_v6().value().address, v6_address);
}

TEST(DnsSdDnsDataTests, TestConvertDnsDataBadTxt) {
  DnsDataTesting data = CreateFullyPopulatedData();
  data.set_txt(cast::mdns::TxtRecordRdata{"=bad_text"});
  ErrorOr<DnsSdInstanceRecord> result = data.CreateRecord();
  EXPECT_TRUE(result.is_error());
}

// ApplyDataRecordChange tests.
TEST(DnsSdDnsDataTests, TestApplyRecordChanges) {
  cast::mdns::MdnsRecord record = CreateFullyPopulatedRecord(port_num);
  InstanceKey instance{instance_name, service_name, domain_name};
  DnsDataTesting data(instance);
  EXPECT_TRUE(data.ApplyDataRecordChange(
                      record, cast::mdns::RecordChangedEvent::kCreated)
                  .ok());
  ASSERT_TRUE(data.srv().has_value());
  EXPECT_EQ(data.srv().value().port(), port_num);

  record = CreateFullyPopulatedRecord(234);
  EXPECT_FALSE(data.ApplyDataRecordChange(
                       record, cast::mdns::RecordChangedEvent::kCreated)
                   .ok());
  ASSERT_TRUE(data.srv().has_value());
  EXPECT_EQ(data.srv().value().port(), port_num);

  record = CreateFullyPopulatedRecord(345);
  EXPECT_TRUE(data.ApplyDataRecordChange(
                      record, cast::mdns::RecordChangedEvent::kUpdated)
                  .ok());
  ASSERT_TRUE(data.srv().has_value());
  EXPECT_EQ(data.srv().value().port(), 345);

  record = CreateFullyPopulatedRecord();
  EXPECT_TRUE(data.ApplyDataRecordChange(
                      record, cast::mdns::RecordChangedEvent::kDeleted)
                  .ok());
  ASSERT_FALSE(data.srv().has_value());

  record = CreateFullyPopulatedRecord(1234);
  EXPECT_FALSE(data.ApplyDataRecordChange(
                       record, cast::mdns::RecordChangedEvent::kUpdated)
                   .ok());
  ASSERT_FALSE(data.srv().has_value());
}

}  // namespace discovery
}  // namespace openscreen
