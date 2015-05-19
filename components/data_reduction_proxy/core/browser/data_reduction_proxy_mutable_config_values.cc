// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

namespace data_reduction_proxy {

scoped_ptr<DataReductionProxyMutableConfigValues>
DataReductionProxyMutableConfigValues::CreateFromParams(
    const DataReductionProxyParams* params) {
  scoped_ptr<DataReductionProxyMutableConfigValues> config_values(
      new DataReductionProxyMutableConfigValues());
  config_values->promo_allowed_ = params->promo_allowed();
  config_values->holdback_ = params->holdback();
  config_values->allowed_ = params->allowed();
  config_values->fallback_allowed_ = params->fallback_allowed();
  config_values->secure_proxy_check_url_ = params->secure_proxy_check_url();
  return config_values.Pass();
}

DataReductionProxyMutableConfigValues::DataReductionProxyMutableConfigValues()
    : promo_allowed_(false),
      holdback_(false),
      allowed_(false),
      fallback_allowed_(false) {
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyMutableConfigValues::
    ~DataReductionProxyMutableConfigValues() {
}

bool DataReductionProxyMutableConfigValues::promo_allowed() const {
  return promo_allowed_;
}

bool DataReductionProxyMutableConfigValues::holdback() const {
  return holdback_;
}

bool DataReductionProxyMutableConfigValues::allowed() const {
  return allowed_;
}

bool DataReductionProxyMutableConfigValues::fallback_allowed() const {
  return fallback_allowed_;
}

bool DataReductionProxyMutableConfigValues::alternative_allowed() const {
  return false;
}

bool DataReductionProxyMutableConfigValues::alternative_fallback_allowed()
    const {
  return false;
}

bool DataReductionProxyMutableConfigValues::UsingHTTPTunnel(
    const net::HostPortPair& proxy_server) const {
  return false;
}

const std::vector<net::ProxyServer>&
DataReductionProxyMutableConfigValues::proxies_for_http(
    bool use_alternative_configuration) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return use_alternative_configuration ? empty_proxies_ : proxies_for_http_;
}

const std::vector<net::ProxyServer>&
DataReductionProxyMutableConfigValues::proxies_for_https(
    bool use_alternative_configuration) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return use_alternative_configuration ? empty_proxies_ : proxies_for_https_;
}

const GURL& DataReductionProxyMutableConfigValues::secure_proxy_check_url()
    const {
  return secure_proxy_check_url_;
}

void DataReductionProxyMutableConfigValues::UpdateValues(
    const std::vector<net::ProxyServer>& proxies_for_http) {
  DCHECK(thread_checker_.CalledOnValidThread());
  proxies_for_http_ = proxies_for_http;
}

}  // namespace data_reduction_proxy
