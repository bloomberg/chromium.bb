// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/service_impl.h"

#include <utility>

#include "discovery/common/config.h"
#include "discovery/mdns/public/mdns_service.h"
#include "platform/api/task_runner.h"

namespace openscreen {
namespace discovery {

// static
SerialDeletePtr<DnsSdService> CreateDnsSdService(
    TaskRunner* task_runner,
    ReportingClient* reporting_client,
    const Config& config) {
  return SerialDeletePtr<DnsSdService>(
      task_runner, new ServiceImpl(task_runner, reporting_client, config));
}

ServiceImpl::ServiceImpl(TaskRunner* task_runner,
                         ReportingClient* reporting_client,
                         const Config& config)
    : task_runner_(task_runner),
      mdns_service_(MdnsService::Create(
          task_runner,
          reporting_client,
          config,
          config.network_info[0].interface.index,
          config.network_info[0].supported_address_families)),
      network_config_(config.network_info[0].interface.index,
                      config.network_info[0].interface.GetIpAddressV4(),
                      config.network_info[0].interface.GetIpAddressV6()) {
  OSP_DCHECK_EQ(config.network_info.size(), 1);
  const auto supported_address_families =
      config.network_info[0].supported_address_families;

  OSP_DCHECK((supported_address_families & Config::NetworkInfo::kUseIpV4) |
             !network_config_.HasAddressV4());
  OSP_DCHECK((supported_address_families & Config::NetworkInfo::kUseIpV6) |
             !network_config_.HasAddressV6());

  if (config.enable_querying) {
    querier_ = std::make_unique<QuerierImpl>(mdns_service_.get(), task_runner_,
                                             &network_config_);
  }
  if (config.enable_publication) {
    publisher_ = std::make_unique<PublisherImpl>(
        mdns_service_.get(), reporting_client, task_runner_, &network_config_);
  }
}

ServiceImpl::~ServiceImpl() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
}

}  // namespace discovery
}  // namespace openscreen
