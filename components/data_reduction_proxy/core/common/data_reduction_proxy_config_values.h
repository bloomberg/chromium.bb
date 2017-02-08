// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CONFIG_VALUES_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CONFIG_VALUES_H_

#include <vector>

class GURL;

namespace data_reduction_proxy {

class DataReductionProxyServer;

class DataReductionProxyConfigValues {
 public:
  virtual ~DataReductionProxyConfigValues() {}

  // Returns true if the data reduction proxy promo may be shown.
  // This is independent of whether the data reduction proxy is allowed.
  // TODO(bengr): maybe tie to whether proxy is allowed.
  virtual bool promo_allowed() const = 0;

  // Returns true if the data reduction proxy should not actually use the
  // proxy if enabled.
  virtual bool holdback() const = 0;

  // Returns the HTTP proxy servers to be used.
  virtual const std::vector<DataReductionProxyServer>& proxies_for_http()
      const = 0;

  // Returns the URL to check to decide if the secure proxy origin should be
  // used.
  virtual const GURL& secure_proxy_check_url() const = 0;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_CONFIG_VALUES_H_
