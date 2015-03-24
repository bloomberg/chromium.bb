// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_CLIENT_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_CLIENT_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"

namespace data_reduction_proxy {

class DataReductionProxyParams;
class DataReductionProxyRequestOptions;

// Retrieves the Data Reduction Proxy configuration from a remote service. This
// object lives on the IO thread.
class DataReductionProxyConfigServiceClient {
 public:
  // The caller must ensure that all parameters remain alive for the lifetime of
  // the |DataReductionProxyConfigClient|.
  DataReductionProxyConfigServiceClient(
      DataReductionProxyParams* params,
      DataReductionProxyRequestOptions* request_options);

  ~DataReductionProxyConfigServiceClient();

 private:
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigClientTest,
                           TestConstructStaticResponse);

  // Constructs a synthetic response based on |params_|.
  std::string ConstructStaticResponse() const;

  // The caller must ensure that the |params_| outlives this instance.
  DataReductionProxyParams* params_;

  // The caller must ensure that the |request_options_| outlives this instance.
  DataReductionProxyRequestOptions* request_options_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfigServiceClient);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_CLIENT_H_
