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
namespace {

MdnsService::SupportedNetworkAddressFamily GetSupportedEndpointTypes(
    const InterfaceInfo& interface) {
  MdnsService::SupportedNetworkAddressFamily supported_types =
      MdnsService::kNoAddressFamily;
  if (interface.GetIpAddressV4()) {
    supported_types = supported_types | MdnsService::kUseIpV4Multicast;
  }
  if (interface.GetIpAddressV6()) {
    supported_types = supported_types | MdnsService::kUseIpV6Multicast;
  }
  return supported_types;
}

}  // namespace

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
      mdns_service_(
          MdnsService::Create(task_runner,
                              reporting_client,
                              config,
                              config.interface.index,
                              GetSupportedEndpointTypes(config.interface))) {
  if (config.enable_querying) {
    querier_ = std::make_unique<QuerierImpl>(mdns_service_.get(), task_runner_);
  }
  if (config.enable_publication) {
    publisher_ = std::make_unique<PublisherImpl>(
        mdns_service_.get(), reporting_client, task_runner_);
  }
}

ServiceImpl::~ServiceImpl() {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
}

}  // namespace discovery
}  // namespace openscreen
