// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_DNS_PROBE_TEST_UTIL_H_
#define CHROME_BROWSER_NET_DNS_PROBE_TEST_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "net/dns/dns_client.h"
#include "net/dns/dns_test_util.h"

namespace chrome_browser_net {

// Creates a mock DNS client with a single rule for the known-good query
// (currently google.com) that returns |result|.
scoped_ptr<net::DnsClient> CreateMockDnsClientForProbes(
    net::MockDnsClientRule::Result result);

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_DNS_PROBE_TEST_UTIL_H_
