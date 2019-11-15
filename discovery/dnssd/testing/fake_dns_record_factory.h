// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_TESTING_FAKE_DNS_RECORD_FACTORY_H_
#define DISCOVERY_DNSSD_TESTING_FAKE_DNS_RECORD_FACTORY_H_

#include <chrono>
#include <string>

#include "discovery/dnssd/impl/constants.h"
#include "discovery/dnssd/impl/instance_key.h"
#include "discovery/mdns/mdns_records.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace discovery {

class FakeDnsRecordFactory {
 public:
  static constexpr uint16_t kPortNum = 80;
  static const IPAddress kV4Address;
  static const IPAddress kV6Address;
  static const IPEndpoint kV4Endpoint;
  static const IPEndpoint kV6Endpoint;
  static const std::string kInstanceName;
  static const std::string kServiceName;
  static const std::string kServiceNameProtocolPart;
  static const std::string kServiceNameServicePart;
  static const std::string kDomainName;
  static const InstanceKey kKey;

  static MdnsRecord CreateFullyPopulatedSrvRecord(uint16_t port = kPortNum);
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_TESTING_FAKE_DNS_RECORD_FACTORY_H_
