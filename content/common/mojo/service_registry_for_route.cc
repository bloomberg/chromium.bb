// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/mojo/service_registry_for_route.h"

#include <utility>

#include "content/common/mojo/service_registry_impl.h"

namespace content {

ServiceRegistryForRoute::~ServiceRegistryForRoute() {
  if (!destruction_callback_.is_null())
    destruction_callback_.Run();
}

void ServiceRegistryForRoute::Add(
    const std::string& service_name,
    const base::Callback<void(mojo::ScopedMessagePipeHandle)>
        service_factory) {
  if (!registry_)
    return;
  registry_->AddRouted(routing_id_, service_name, service_factory);
}

void ServiceRegistryForRoute::Remove(const std::string& service_name) {
  if (!registry_)
    return;
  registry_->RemoveRouted(routing_id_, service_name);
}

void ServiceRegistryForRoute::Connect(const base::StringPiece& name,
                                      mojo::ScopedMessagePipeHandle handle) {
  if (!registry_)
    return;
  registry_->ConnectRouted(routing_id_, name.as_string(), std::move(handle));
}

ServiceRegistryForRoute::ServiceRegistryForRoute(
    base::WeakPtr<ServiceRegistryImpl> registry,
    int routing_id,
    const base::Closure& destruction_callback)
    : registry_(registry),
      routing_id_(routing_id),
      destruction_callback_(destruction_callback) {
}

}  // namespace content
