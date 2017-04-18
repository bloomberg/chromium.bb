// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/simple_connection_filter.h"

#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service_info.h"

namespace content {

SimpleConnectionFilter::SimpleConnectionFilter(
    std::unique_ptr<service_manager::BinderRegistry> registry)
    : registry_(std::move(registry)) {}

SimpleConnectionFilter::~SimpleConnectionFilter() {}

void SimpleConnectionFilter::OnBindInterface(
    const service_manager::ServiceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe,
    service_manager::Connector* connector) {
  if (registry_->CanBindInterface(interface_name)) {
    registry_->BindInterface(source_info.identity, interface_name,
                             std::move(*interface_pipe));
  }
}

}  // namespace content
