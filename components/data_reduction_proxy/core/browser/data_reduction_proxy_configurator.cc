// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"

#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_event_creator.h"
#include "net/proxy/proxy_config.h"

namespace data_reduction_proxy {

DataReductionProxyConfigurator::DataReductionProxyConfigurator(
    net::NetLog* net_log,
    DataReductionProxyEventCreator* event_creator)
    : net_log_(net_log), data_reduction_proxy_event_creator_(event_creator) {
  DCHECK(net_log);
  DCHECK(event_creator);
  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyConfigurator::~DataReductionProxyConfigurator() {
}

void DataReductionProxyConfigurator::Enable(
    bool primary_restricted,
    bool fallback_restricted,
    const std::string& primary_origin,
    const std::string& fallback_origin,
    const std::string& ssl_origin) {
  DCHECK(thread_checker_.CalledOnValidThread());
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
  data_reduction_proxy_event_creator_->AddProxyEnabledEvent(
      net_log_, primary_restricted, fallback_restricted, primary_origin,
      fallback_origin, ssl_origin);
  config_ = config;
}

void DataReductionProxyConfigurator::Disable() {
  DCHECK(thread_checker_.CalledOnValidThread());
  net::ProxyConfig config = net::ProxyConfig::CreateDirect();
  data_reduction_proxy_event_creator_->AddProxyDisabledEvent(net_log_);
  config_ = config;
}

void DataReductionProxyConfigurator::AddHostPatternToBypass(
    const std::string& pattern) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bypass_rules_.push_back(pattern);
}

void DataReductionProxyConfigurator::AddURLPatternToBypass(
    const std::string& pattern) {
  DCHECK(thread_checker_.CalledOnValidThread());
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

const net::ProxyConfig& DataReductionProxyConfigurator::GetProxyConfig() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return config_;
}

}  // namespace data_reduction_proxy
