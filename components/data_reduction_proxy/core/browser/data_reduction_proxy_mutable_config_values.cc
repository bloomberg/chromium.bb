// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"

#include "base/single_thread_task_runner.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

namespace data_reduction_proxy {

scoped_ptr<DataReductionProxyMutableConfigValues>
DataReductionProxyMutableConfigValues::CreateFromParams(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    const DataReductionProxyParams* params) {
  scoped_ptr<DataReductionProxyMutableConfigValues> config_values(
      new DataReductionProxyMutableConfigValues(io_task_runner));
  config_values->promo_allowed_ = params->promo_allowed();
  config_values->holdback_ = params->holdback();
  config_values->allowed_ = params->allowed();
  config_values->fallback_allowed_ = params->fallback_allowed();
  config_values->secure_proxy_check_url_ = params->secure_proxy_check_url();
  return config_values.Pass();
}

DataReductionProxyMutableConfigValues::DataReductionProxyMutableConfigValues(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : empty_origin_(),
      promo_allowed_(false),
      holdback_(false),
      allowed_(false),
      fallback_allowed_(false),
      origin_(empty_origin_),
      fallback_origin_(empty_origin_),
      io_task_runner_(io_task_runner) {
  DCHECK(io_task_runner.get());
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

bool DataReductionProxyMutableConfigValues::IsDataReductionProxy(
    const net::HostPortPair& host_port_pair,
    DataReductionProxyTypeInfo* proxy_info) const {
  // TODO(jeremyim): Rework as part of ConfigValues interface changes.
  if (allowed() && origin().is_valid() &&
      origin().host_port_pair().Equals(host_port_pair)) {
    if (proxy_info) {
      proxy_info->proxy_servers.first = origin();
      if (fallback_allowed())
        proxy_info->proxy_servers.second = fallback_origin();
    }
    return true;
  }

  if (!fallback_allowed() || !fallback_origin().is_valid() ||
      !fallback_origin().host_port_pair().Equals(host_port_pair))
    return false;

  if (proxy_info) {
    proxy_info->proxy_servers.first = fallback_origin();
    proxy_info->proxy_servers.second = net::ProxyServer::FromURI(
        std::string(), net::ProxyServer::SCHEME_HTTP);
    proxy_info->is_fallback = true;
  }

  return true;
}

const net::ProxyServer& DataReductionProxyMutableConfigValues::origin() const {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  return origin_;
}

const net::ProxyServer& DataReductionProxyMutableConfigValues::fallback_origin()
    const {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  return fallback_origin_;
}

const net::ProxyServer& DataReductionProxyMutableConfigValues::alt_origin()
    const {
  return empty_origin_;
}

const net::ProxyServer&
DataReductionProxyMutableConfigValues::alt_fallback_origin() const {
  return empty_origin_;
}

const net::ProxyServer& DataReductionProxyMutableConfigValues::ssl_origin()
    const {
  return empty_origin_;
}

const GURL& DataReductionProxyMutableConfigValues::secure_proxy_check_url()
    const {
  return secure_proxy_check_url_;
}

void DataReductionProxyMutableConfigValues::UpdateValues(
    const net::ProxyServer& origin,
    const net::ProxyServer& fallback_origin) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  origin_ = origin;
  fallback_origin_ = fallback_origin;
}

}  // namespace data_reduction_proxy
