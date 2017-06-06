// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_TEST_UTILS_H_

#include <vector>

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

namespace data_reduction_proxy {

class DataReductionProxyServer;

class TestDataReductionProxyParams : public DataReductionProxyParams {
 public:
  TestDataReductionProxyParams();
  bool init_result() const;

  void SetProxiesForHttp(const std::vector<DataReductionProxyServer>& proxies);

  // Test values to replace the values specified in preprocessor defines.
  static std::string DefaultOrigin();
  static std::string DefaultFallbackOrigin();
  static std::string DefaultSecureProxyCheckURL();

  static std::string FlagOrigin();
  static std::string FlagFallbackOrigin();
  static std::string FlagSecureProxyCheckURL();

 protected:
  std::string GetDefaultOrigin() const override;

  std::string GetDefaultFallbackOrigin() const override;

  std::string GetDefaultSecureProxyCheckURL() const override;

 private:
  bool init_result_;
};
}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_PARAMS_TEST_UTILS_H_
