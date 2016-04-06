// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/mojo/blink_service_registry_impl.h"

#include <utility>

#include "content/common/mojo/service_registry_impl.h"

namespace content {

BlinkServiceRegistryImpl::BlinkServiceRegistryImpl(
    base::WeakPtr<content::ServiceRegistry> service_registry)
    : service_registry_(service_registry) {}

BlinkServiceRegistryImpl::~BlinkServiceRegistryImpl() = default;

void BlinkServiceRegistryImpl::connectToRemoteService(
    const char* name,
    mojo::ScopedMessagePipeHandle handle) {
  if (!service_registry_)
    return;

  service_registry_->ConnectToRemoteService(name, std::move(handle));
}

}  // namespace content
