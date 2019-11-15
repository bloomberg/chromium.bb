// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_TESTING_FAKE_DNS_RECORD_FACTORY_H_
#define DISCOVERY_DNSSD_TESTING_FAKE_DNS_RECORD_FACTORY_H_

#include <chrono>

#include "discovery/dnssd/impl/constants.h"
#include "discovery/dnssd/impl/instance_key.h"
#include "discovery/dnssd/public/instance_record.h"
#include "discovery/dnssd/public/txt_record.h"
#include "discovery/mdns/mdns_records.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace openscreen {
namespace discovery {

class FakeDnsRecordFactory {
 public:
  static const IPAddress v4_address;
  static const IPAddress v6_address;
  static const std::string instance_name;
  static const std::string service_name;
  static const std::string domain_name;
  static const InstanceKey key;
  static constexpr uint16_t port_num = uint16_t{80};

  static MdnsRecord CreateFullyPopulatedSrvRecord(uint16_t port = port_num);
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_TESTING_FAKE_DNS_RECORD_FACTORY_H_
