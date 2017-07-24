// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_MUTABLE_CONFIG_VALUES_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_MUTABLE_CONFIG_VALUES_H_

#include <vector>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_config_values.h"
#include "net/proxy/proxy_server.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

class DataReductionProxyServer;

// A |DataReductionProxyConfigValues| which is permitted to change its
// underlying values via the UpdateValues method.
class DataReductionProxyMutableConfigValues
    : public DataReductionProxyConfigValues {
 public:
  DataReductionProxyMutableConfigValues();
  ~DataReductionProxyMutableConfigValues() override;

  // Updates |proxies_for_http_| with the provided values.
  // Virtual for testing.
  virtual void UpdateValues(
      const std::vector<DataReductionProxyServer>& proxies_for_http);

  // Invalidates |this| by clearing the stored Data Reduction Proxy servers.
  void Invalidate();

  // Overrides of |DataReductionProxyConfigValues|
  const std::vector<DataReductionProxyServer>& proxies_for_http()
      const override;

 private:
  std::vector<DataReductionProxyServer> proxies_for_http_;
  std::vector<DataReductionProxyServer> override_proxies_for_http_;

  // Permits use of locally specified Data Reduction Proxy servers instead of
  // ones specified from the Data Saver API.
  const bool use_override_proxies_for_http_;

  // Enforce usage on the IO thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyMutableConfigValues);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_MUTABLE_CONFIG_VALUES_H_
