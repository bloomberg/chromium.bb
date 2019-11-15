// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/testing/fake_dns_record_factory.h"

namespace openscreen {
namespace discovery {

// static
MdnsRecord FakeDnsRecordFactory::CreateFullyPopulatedSrvRecord(uint16_t port) {
  DomainName target{instance_name, "_srv-name", "_udp", domain_name};
  auto type = DnsType::kSRV;
  auto clazz = DnsClass::kIN;
  auto record_type = RecordType::kUnique;
  auto ttl = std::chrono::seconds(0);
  SrvRecordRdata srv(0, 0, port, target);
  return MdnsRecord(target, type, clazz, record_type, ttl, srv);
}

// static
const IPAddress FakeDnsRecordFactory::v4_address =
    IPAddress(std::array<uint8_t, 4>{{192, 168, 0, 0}});

// static
const IPAddress FakeDnsRecordFactory::v6_address =
    IPAddress(std::array<uint8_t, 16>{
        {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}});

// static
const std::string FakeDnsRecordFactory::instance_name = "instance";

// static
const std::string FakeDnsRecordFactory::service_name = "_srv-name._udp";

// static
const std::string FakeDnsRecordFactory::domain_name = "local";

// static
const InstanceKey FakeDnsRecordFactory::key =
    InstanceKey(instance_name, service_name, domain_name);

}  // namespace discovery
}  // namespace openscreen
