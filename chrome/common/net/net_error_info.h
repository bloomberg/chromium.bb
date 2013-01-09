// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_NET_ERROR_INFO_H_
#define CHROME_COMMON_NET_NET_ERROR_INFO_H_

namespace chrome_common_net {

enum DnsProbeResult {
  DNS_PROBE_UNKNOWN,
  DNS_PROBE_NO_INTERNET,
  DNS_PROBE_BAD_CONFIG,
  DNS_PROBE_NXDOMAIN,
  DNS_PROBE_MAX
};

}  // namespace chrome_common_net

#endif  // CHROME_COMMON_NET_NET_ERROR_INFO_H_
