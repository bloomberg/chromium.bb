// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/service_impl.h"

#include <utility>

#include "discovery/mdns/public/mdns_service.h"

namespace openscreen {
namespace discovery {

// static
std::unique_ptr<DnsSdService> DnsSdService::Create(
    TaskRunner* task_runner,
    ReportingClient* reporting_client,
    const Config& config) {
  return std::make_unique<ServiceImpl>(task_runner, reporting_client, config);
}

ServiceImpl::ServiceImpl(TaskRunner* task_runner,
                         ReportingClient* reporting_client,
                         const Config& config)
    : mdns_service_(MdnsService::Create(task_runner, reporting_client, config)),
      querier_(mdns_service_.get()),
      publisher_(mdns_service_.get(), reporting_client) {}

ServiceImpl::~ServiceImpl() = default;

}  // namespace discovery
}  // namespace openscreen
