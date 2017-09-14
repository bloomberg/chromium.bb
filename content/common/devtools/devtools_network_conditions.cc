// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/devtools/devtools_network_conditions.h"

namespace content {

DevToolsNetworkConditions::DevToolsNetworkConditions()
    : DevToolsNetworkConditions(false) {}

DevToolsNetworkConditions::DevToolsNetworkConditions(bool offline)
    : DevToolsNetworkConditions(offline, 0, 0, 0) {}

DevToolsNetworkConditions::DevToolsNetworkConditions(bool offline,
                                                     double latency,
                                                     double download_throughput,
                                                     double upload_throughput)
    : offline_(offline),
      latency_(latency),
      download_throughput_(download_throughput),
      upload_throughput_(upload_throughput) {}

DevToolsNetworkConditions::~DevToolsNetworkConditions() {}

bool DevToolsNetworkConditions::IsThrottling() const {
  return !offline_ && ((latency_ != 0) || (download_throughput_ != 0.0) ||
                       (upload_throughput_ != 0));
}

}  // namespace content
