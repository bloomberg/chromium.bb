// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"

#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_store.h"
#include "net/proxy/proxy_config.h"

namespace data_reduction_proxy {

DataReductionProxyConfigurator::DataReductionProxyConfigurator(
    scoped_refptr<base::SequencedTaskRunner> network_task_runner,
    net::NetLog* net_log,
    DataReductionProxyEventStore* event_store)
    : network_task_runner_(network_task_runner),
      net_log_(net_log),
      data_reduction_proxy_event_store_(event_store) {
  DCHECK(network_task_runner.get());
  DCHECK(net_log);
  DCHECK(event_store);
}

DataReductionProxyConfigurator::~DataReductionProxyConfigurator() {
}

void DataReductionProxyConfigurator::Enable(
    bool primary_restricted,
    bool fallback_restricted,
    const std::string& primary_origin,
    const std::string& fallback_origin,
    const std::string& ssl_origin) {
  std::vector<std::string> proxies;
  if (!primary_restricted) {
    std::string trimmed_primary;
    base::TrimString(primary_origin, "/", &trimmed_primary);
    if (!trimmed_primary.empty())
      proxies.push_back(trimmed_primary);
  }
  if (!fallback_restricted) {
    std::string trimmed_fallback;
    base::TrimString(fallback_origin, "/", &trimmed_fallback);
    if (!trimmed_fallback.empty())
      proxies.push_back(trimmed_fallback);
  }
  if (proxies.empty()) {
    Disable();
    return;
  }

  std::string trimmed_ssl;
  base::TrimString(ssl_origin, "/", &trimmed_ssl);

  std::string server = "http=" + JoinString(proxies, ",") + ",direct://;"
      + (ssl_origin.empty() ? "" : ("https=" + trimmed_ssl + ",direct://;"));

  net::ProxyConfig config;
  config.proxy_rules().ParseFromString(server);
  config.proxy_rules().bypass_rules.ParseFromString(
      JoinString(bypass_rules_, ", "));
  // The ID is set to a bogus value. It cannot be left uninitialized, else the
  // config will return invalid.
  net::ProxyConfig::ID unused_id = 1;
  config.set_id(unused_id);
  data_reduction_proxy_event_store_->AddProxyEnabledEvent(
      net_log_, primary_restricted, fallback_restricted, primary_origin,
      fallback_origin, ssl_origin);
  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &DataReductionProxyConfigurator::UpdateProxyConfigOnIOThread,
          base::Unretained(this),
          config));
}

void DataReductionProxyConfigurator::Disable() {
  net::ProxyConfig config = net::ProxyConfig::CreateDirect();
  data_reduction_proxy_event_store_->AddProxyDisabledEvent(net_log_);
  network_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &DataReductionProxyConfigurator::UpdateProxyConfigOnIOThread,
          base::Unretained(this),
          config));
}

void DataReductionProxyConfigurator::AddHostPatternToBypass(
    const std::string& pattern) {
  bypass_rules_.push_back(pattern);
}

void DataReductionProxyConfigurator::AddURLPatternToBypass(
    const std::string& pattern) {
  size_t pos = pattern.find('/');
  if (pattern.find('/', pos + 1) == pos + 1)
    pos = pattern.find('/', pos + 2);

  std::string host_pattern;
  if (pos != std::string::npos)
    host_pattern = pattern.substr(0, pos);
  else
    host_pattern = pattern;

  AddHostPatternToBypass(host_pattern);
}

void DataReductionProxyConfigurator::UpdateProxyConfigOnIOThread(
    const net::ProxyConfig& config) {
  config_ = config;
}

const net::ProxyConfig&
DataReductionProxyConfigurator::GetProxyConfigOnIOThread() const {
  return config_;
}

}  // namespace data_reduction_proxy
