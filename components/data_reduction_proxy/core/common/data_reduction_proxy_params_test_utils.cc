// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"

namespace {
// Test values to replace the values specified in preprocessor defines.
static const char kDefaultOrigin[] = "origin.net:80";
static const char kDefaultFallbackOrigin[] = "fallback.net:80";

static const char kFlagOrigin[] = "https://origin.org:443";
static const char kFlagFallbackOrigin[] = "fallback.org:80";
}

namespace data_reduction_proxy {
TestDataReductionProxyParams::TestDataReductionProxyParams()
    : DataReductionProxyParams(false) {
  init_result_ = Init();
  }

bool TestDataReductionProxyParams::init_result() const {
  return init_result_;
}

void TestDataReductionProxyParams::SetProxiesForHttp(
    const std::vector<DataReductionProxyServer>& proxies) {
  SetProxiesForHttpForTesting(proxies);
}
// Test values to replace the values specified in preprocessor defines.
std::string TestDataReductionProxyParams::DefaultOrigin() {
  return kDefaultOrigin;
}

std::string TestDataReductionProxyParams::DefaultFallbackOrigin() {
  return kDefaultFallbackOrigin;
}

std::string TestDataReductionProxyParams::FlagOrigin() {
  return kFlagOrigin;
}

std::string TestDataReductionProxyParams::FlagFallbackOrigin() {
  return kFlagFallbackOrigin;
}

std::string TestDataReductionProxyParams::GetDefaultOrigin() const {
  return kDefaultOrigin;
}

std::string TestDataReductionProxyParams::GetDefaultFallbackOrigin() const {
  return kDefaultFallbackOrigin;
}

}  // namespace data_reduction_proxy
