// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_throttle_manager.h"

#include "base/memory/ptr_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"

namespace net {
class HttpRequestHeaders;
}

namespace data_reduction_proxy {

DataReductionProxyThrottleManager::DataReductionProxyThrottleManager(
    mojom::DataReductionProxy* data_reduction_proxy,
    mojom::DataReductionProxyThrottleConfigPtr initial_config)
    : data_reduction_proxy_(data_reduction_proxy), binding_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojom::DataReductionProxyThrottleConfigObserverPtr observer;
  binding_.Bind(mojo::MakeRequest(&observer));

  data_reduction_proxy_->AddThrottleConfigObserver(std::move(observer));

  OnThrottleConfigChanged(std::move(initial_config));
}

DataReductionProxyThrottleManager::~DataReductionProxyThrottleManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::Optional<DataReductionProxyTypeInfo>
DataReductionProxyThrottleManager::FindConfiguredDataReductionProxy(
    const net::ProxyServer& proxy_server) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(https://crbug.com/721403): The non-NS code also searches through the
  // recently seen proxies, not just the current ones.
  return params::FindConfiguredProxyInVector(proxies_for_http_, proxy_server);
}

void DataReductionProxyThrottleManager::MarkProxiesAsBad(
    base::TimeDelta bypass_duration,
    const net::ProxyList& bad_proxies,
    mojom::DataReductionProxy::MarkProxiesAsBadCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // There is no need to handle the case where |callback| is never invoked
  // (possible on connection error). That would imply disconnection from the
  // browser, which is not recoverable.
  data_reduction_proxy_->MarkProxiesAsBad(bypass_duration, bad_proxies,
                                          std::move(callback));
}

void DataReductionProxyThrottleManager::OnThrottleConfigChanged(
    mojom::DataReductionProxyThrottleConfigPtr config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  proxies_for_http_.clear();

  if (!config)
    return;

  // TODO(eroman): Use typemappings instead of converting here?
  for (const auto& entry : config->proxies_for_http) {
    proxies_for_http_.push_back(DataReductionProxyServer(entry->proxy_server));
  }
}

// static
mojom::DataReductionProxyThrottleConfigPtr
DataReductionProxyThrottleManager::CreateConfig(
    const std::vector<DataReductionProxyServer>& proxies_for_http) {
  auto config = mojom::DataReductionProxyThrottleConfig::New();

  for (const auto& drp_server : proxies_for_http) {
    auto converted = mojom::DataReductionProxyServer::New();
    converted->is_core = drp_server.IsCoreProxy();
    converted->proxy_server = drp_server.proxy_server();

    config->proxies_for_http.push_back(std::move(converted));
  }

  return config;
}

}  // namespace data_reduction_proxy
