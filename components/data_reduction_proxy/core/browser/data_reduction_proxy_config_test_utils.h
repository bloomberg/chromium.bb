// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_

#include "base/memory/scoped_ptr.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "net/base/net_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {
class URLFetcher;
}

namespace data_reduction_proxy {

class MockDataReductionProxyConfig : public DataReductionProxyConfig {
 public:
  MockDataReductionProxyConfig();
  MockDataReductionProxyConfig(int flags);
  ~MockDataReductionProxyConfig() override;

  MOCK_METHOD0(GetURLFetcherForProbe, net::URLFetcher*());
  MOCK_METHOD1(RecordProbeURLFetchResult, void(ProbeURLFetchResult result));
  MOCK_METHOD3(LogProxyState,
               void(bool enabled, bool restricted, bool at_startup));

  // SetProxyConfigs should always call LogProxyState exactly once.
  virtual void SetProxyConfigs(bool enabled,
                               bool alternative_enabled,
                               bool restricted,
                               bool at_startup) override;

  virtual void GetNetworkList(net::NetworkInterfaceList* interfaces,
                              int policy) override;

  net::NetworkInterfaceList* interfaces() {
    return network_interfaces_.get();
  }

 private:
  scoped_ptr<net::NetworkInterfaceList> network_interfaces_;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_
