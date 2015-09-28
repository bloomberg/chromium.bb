// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_SWITCHES_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_SWITCHES_H_

namespace data_reduction_proxy {
namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.

extern const char kDataReductionProxy[];
extern const char kDataReductionProxyDev[];
extern const char kDataReductionProxyExperiment[];
extern const char kDataReductionProxyFallback[];
extern const char kDataReductionProxyKey[];
extern const char kDataReductionProxySecureProxyCheckURL[];
extern const char kDataReductionProxyStartSecureDisabled[];
extern const char kDataReductionProxyWarmupURL[];
extern const char kDataReductionSSLProxy[];
extern const char kDataReductionProxyLoFi[];
extern const char kDataReductionProxyLoFiValueAlwaysOn[];
extern const char kDataReductionProxyLoFiValueCellularOnly[];
extern const char kDataReductionProxyLoFiValueDisabled[];
extern const char kDataReductionProxyLoFiValueSlowConnectionsOnly[];
extern const char kDisableDataReductionProxyDev[];
extern const char kEnableDataReductionProxyDev[];
extern const char kEnableDataReductionProxyCarrierTest[];
extern const char kEnableDataReductionProxy[];
extern const char kEnableDataReductionProxyBypassWarning[];
extern const char kClearDataReductionProxyDataSavings[];
extern const char kEnableDataReductionProxyConfigClient[];
extern const char kDataReductionProxyConfigURL[];
extern const char kDataReductionProxyHttpProxies[];

}  // namespace switches
}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_REDUCTION_PROXY_SWITCHES_H_
