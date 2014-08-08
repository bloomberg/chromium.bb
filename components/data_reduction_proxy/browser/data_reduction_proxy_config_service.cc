// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_config_service.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"

namespace data_reduction_proxy {

DataReductionProxyConfigService::DataReductionProxyConfigService(
    scoped_ptr<net::ProxyConfigService> base_service)
    : config_read_pending_(true),
      registered_observer_(false),
      enabled_(false),
      restricted_(false) {
  base_service_ = base_service.Pass();
}

DataReductionProxyConfigService::~DataReductionProxyConfigService() {
  if (registered_observer_ && base_service_.get())
    base_service_->RemoveObserver(this);
}

void DataReductionProxyConfigService::AddObserver(
    net::ProxyConfigService::Observer* observer) {
  RegisterObserver();
  observers_.AddObserver(observer);
}

void DataReductionProxyConfigService::RemoveObserver(
    net::ProxyConfigService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

net::ProxyConfigService::ConfigAvailability
DataReductionProxyConfigService::GetLatestProxyConfig(
    net::ProxyConfig* config) {
  RegisterObserver();

  if (enabled_) {
    *config = config_;
    return net::ProxyConfigService::CONFIG_VALID;
  }

  // Ask the base service if available.
  net::ProxyConfig system_config;
  ConfigAvailability system_availability =
      net::ProxyConfigService::CONFIG_UNSET;
  if (base_service_.get()) {
    system_availability = base_service_->GetLatestProxyConfig(&system_config);
    *config = system_config;
  }
  if (system_availability == net::ProxyConfigService::CONFIG_UNSET) {
    *config = net::ProxyConfig::CreateDirect();
  }

  return net::ProxyConfigService::CONFIG_VALID;
}

void DataReductionProxyConfigService::OnLazyPoll() {
  if (base_service_.get())
    base_service_->OnLazyPoll();
}

void DataReductionProxyConfigService::UpdateProxyConfig(
    bool enabled,
    const net::ProxyConfig& config) {

  config_read_pending_ = false;
  enabled_ = enabled;
  config_ = config;

  if (!observers_.might_have_observers())
    return;

  // Evaluate the proxy configuration. If GetLatestProxyConfig returns
  // CONFIG_PENDING, we are using the system proxy service, but it doesn't have
  // a valid configuration yet. Once it is ready, OnProxyConfigChanged() will be
  // called and broadcast the proxy configuration.
  // Note: If a switch between a preference proxy configuration and the system
  // proxy configuration occurs an unnecessary notification might get sent if
  // the two configurations agree. This case should be rare however, so we don't
  // handle that case specially.
  net::ProxyConfig new_config;
  ConfigAvailability availability = GetLatestProxyConfig(&new_config);
  if (availability != CONFIG_PENDING) {
    FOR_EACH_OBSERVER(net::ProxyConfigService::Observer, observers_,
                      OnProxyConfigChanged(new_config, availability));
  }
}

void DataReductionProxyConfigService::OnProxyConfigChanged(
    const net::ProxyConfig& config,
    ConfigAvailability availability) {

  // Check whether the data reduction proxy is enabled. In this case that proxy
  // configuration takes precedence and the change event from the delegate proxy
  // service can be disregarded.
  if (!enabled_) {
    net::ProxyConfig actual_config;
    availability = GetLatestProxyConfig(&actual_config);
    FOR_EACH_OBSERVER(net::ProxyConfigService::Observer, observers_,
                      OnProxyConfigChanged(actual_config, availability));
  }
}

void DataReductionProxyConfigService::RegisterObserver() {
  if (!registered_observer_ && base_service_.get()) {
    base_service_->AddObserver(this);
    registered_observer_ = true;
  }
}

DataReductionProxyConfigTracker::DataReductionProxyConfigTracker(
    base::Callback<void(bool, const net::ProxyConfig&)> update_proxy_config,
    base::TaskRunner* task_runner)
    : update_proxy_config_(update_proxy_config),
      task_runner_(task_runner) {
}

DataReductionProxyConfigTracker::~DataReductionProxyConfigTracker() {
}

void DataReductionProxyConfigTracker::Enable(
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
    std::string mode;
    Disable();
    return;
  }

  std::string trimmed_ssl;
  base::TrimString(ssl_origin, "/", &trimmed_ssl);

  std::string server = "http=" + JoinString(proxies, ",") + ",direct://;"
      + (ssl_origin.empty() ? "" : ("https=" + ssl_origin + ",direct://;"));

  net::ProxyConfig config;
  config.proxy_rules().ParseFromString(server);
  config.proxy_rules().bypass_rules.ParseFromString(
      JoinString(bypass_rules_, ", "));
  UpdateProxyConfigOnIOThread(true, config);
}


void DataReductionProxyConfigTracker::Disable() {
  net::ProxyConfig config = net::ProxyConfig::CreateDirect();
  UpdateProxyConfigOnIOThread(false, config);
}

void DataReductionProxyConfigTracker::AddHostPatternToBypass(
    const std::string& pattern) {
  bypass_rules_.push_back(pattern);
}

void DataReductionProxyConfigTracker::AddURLPatternToBypass(
    const std::string& pattern) {
  size_t pos = pattern.find("/");
  if (pattern.find("/", pos + 1) == pos + 1)
    pos = pattern.find("/", pos + 2);

  std::string host_pattern;
  if (pos != std::string::npos)
    host_pattern = pattern.substr(0, pos);
  else
    host_pattern = pattern;

  AddHostPatternToBypass(host_pattern);
}

void DataReductionProxyConfigTracker::UpdateProxyConfigOnIOThread(
    bool enabled,
    const net::ProxyConfig& config) {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(update_proxy_config_, enabled, config));
}

}  // namespace data_reduction_proxy
