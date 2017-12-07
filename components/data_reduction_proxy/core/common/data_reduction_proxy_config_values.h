// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CONFIG_VALUES_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CONFIG_VALUES_H_

#include <vector>

namespace data_reduction_proxy {

class DataReductionProxyServer;

class DataReductionProxyConfigValues {
 public:
  virtual ~DataReductionProxyConfigValues() {}

  // Returns the HTTP proxy servers to be used. Proxies that cannot be used
  // because they are temporarily or permanently marked as bad are also
  // included.
  virtual const std::vector<DataReductionProxyServer>& proxies_for_http()
      const = 0;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CONFIG_VALUES_H_
