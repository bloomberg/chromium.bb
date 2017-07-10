// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"

#include <vector>

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"

namespace data_reduction_proxy {

std::unique_ptr<DataReductionProxyMutableConfigValues>
DataReductionProxyMutableConfigValues::CreateFromParams(
    const DataReductionProxyParams* params) {
  std::unique_ptr<DataReductionProxyMutableConfigValues> config_values(
      new DataReductionProxyMutableConfigValues());
  return config_values;
}

DataReductionProxyMutableConfigValues::DataReductionProxyMutableConfigValues()
    : use_override_proxies_for_http_(false) {
  use_override_proxies_for_http_ =
      params::GetOverrideProxiesForHttpFromCommandLine(
          &override_proxies_for_http_);

  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyMutableConfigValues::
    ~DataReductionProxyMutableConfigValues() {
}

const std::vector<DataReductionProxyServer>&
DataReductionProxyMutableConfigValues::proxies_for_http() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (use_override_proxies_for_http_ && !proxies_for_http_.empty()) {
    // Only override the proxies if a non-empty list of proxies would have been
    // returned otherwise. This is to prevent use of the proxies when the config
    // has been invalidated, since attempting to use a Data Reduction Proxy
    // without valid credentials could cause a proxy bypass.
    return override_proxies_for_http_;
  }
  // TODO(sclittle): Support overriding individual proxies in the proxy list
  // according to field trials such as the DRP QUIC field trial and their
  // corresponding command line flags (crbug.com/533637).
  return proxies_for_http_;
}

void DataReductionProxyMutableConfigValues::UpdateValues(
    const std::vector<DataReductionProxyServer>& proxies_for_http) {
  DCHECK(thread_checker_.CalledOnValidThread());
  proxies_for_http_ = proxies_for_http;
}

void DataReductionProxyMutableConfigValues::Invalidate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  proxies_for_http_.clear();
}

}  // namespace data_reduction_proxy
