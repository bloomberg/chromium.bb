// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_TEST_UTILS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"

namespace net {
class NetLog;
}

namespace data_reduction_proxy {

class DataReductionProxyEventStore;

class TestDataReductionProxyConfigurator
    : public DataReductionProxyConfigurator {
 public:
  TestDataReductionProxyConfigurator(
      scoped_refptr<base::SequencedTaskRunner> network_task_runner,
      net::NetLog* net_log,
      DataReductionProxyEventStore* event_store);
  ~TestDataReductionProxyConfigurator() override;

  // Overrides of DataReductionProxyConfigurator
  void Enable(bool restricted,
              bool fallback_restricted,
              const std::string& primary_origin,
              const std::string& fallback_origin,
              const std::string& ssl_origin) override;

  void Disable() override;

  void AddHostPatternToBypass(const std::string& pattern) override {}

  void AddURLPatternToBypass(const std::string& pattern) override {}

  bool enabled() const {
    return enabled_;
  }

  bool restricted() const {
    return restricted_;
  }

  bool fallback_restricted() const {
    return fallback_restricted_;
  }

  const std::string& origin() const {
    return origin_;
  }

  const std::string& fallback_origin() const {
    return fallback_origin_;
  }

  const std::string& ssl_origin() const {
    return ssl_origin_;
  }

 private:
  // True if the proxy has been enabled, i.e., only after |Enable| has been
  // called. Defaults to false.
  bool enabled_;

  // Describes whether the proxy has been put in a restricted mode. True if
  // |Enable| is called with |restricted| set to true. Defaults to false.
  bool restricted_;

  // Describes whether the proxy has been put in a mode where the fallback
  // configuration has been disallowed. True if |Enable| is called with
  // |fallback_restricted| set to true. Defaults to false.
  bool fallback_restricted_;

  // The origins that are passed to |Enable|.
  std::string origin_;
  std::string fallback_origin_;
  std::string ssl_origin_;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIGURATOR_TEST_UTILS_H_
