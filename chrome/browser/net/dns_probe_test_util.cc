// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/dns_probe_test_util.h"

#include "chrome/browser/net/dns_probe_runner.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_protocol.h"

using net::DnsClient;
using net::DnsConfig;
using net::IPAddressNumber;
using net::IPEndPoint;
using net::MockDnsClientRule;
using net::MockDnsClientRuleList;
using net::ParseIPLiteralToNumber;

namespace chrome_browser_net {

scoped_ptr<DnsClient> CreateMockDnsClientForProbes(
    MockDnsClientRule::Result result) {
  DnsConfig config;
  IPAddressNumber dns_ip;
  ParseIPLiteralToNumber("192.168.1.1", &dns_ip);
  const uint16 kDnsPort = net::dns_protocol::kDefaultPort;
  config.nameservers.push_back(IPEndPoint(dns_ip, kDnsPort));

  const uint16 kTypeA = net::dns_protocol::kTypeA;
  MockDnsClientRuleList rules;
  rules.push_back(MockDnsClientRule(DnsProbeRunner::kKnownGoodHostname, kTypeA,
                                    result, false));

  return scoped_ptr<DnsClient>(new net::MockDnsClient(config, rules));
}

}  // namespace chrome_browser_net
