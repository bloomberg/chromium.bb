// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_DEBUG_UI_SERVICE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_DEBUG_UI_SERVICE_H_

#include "base/memory/ref_counted.h"

namespace net {
class ProxyConfig;
}

namespace data_reduction_proxy {

class DataReductionProxyDebugUIManager;

// An interface for the ContentDataReductionProxyDebugUIService which creates
// a DataReductionProxyDebugUIManager that handles showing interstitials. Also
// provides a method to get the Data Reduction Proxy config.
class DataReductionProxyDebugUIService {
 public:
  virtual ~DataReductionProxyDebugUIService() {}

  // Returns the Data Reduction Proxy config.
  virtual const net::ProxyConfig& data_reduction_proxy_config() const = 0;

  // Returns the DataReductionProxyDebugUIManager created and owned by this.
  virtual const scoped_refptr<DataReductionProxyDebugUIManager>&
  ui_manager() const = 0;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_DEBUG_UI_SERVICE_H_
