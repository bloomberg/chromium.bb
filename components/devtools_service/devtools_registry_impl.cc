// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_service/devtools_registry_impl.h"

#include "base/logging.h"

namespace devtools_service {

DevToolsRegistryImpl::DevToolsRegistryImpl(DevToolsService* service)
    : service_(service) {
}

DevToolsRegistryImpl::~DevToolsRegistryImpl() {
}

void DevToolsRegistryImpl::BindToRegistryRequest(
    mojo::InterfaceRequest<DevToolsRegistry> request) {
  bindings_.AddBinding(this, request.Pass());
}

void DevToolsRegistryImpl::RegisterAgent(DevToolsAgentPtr agent) {
  // TODO(yzshen): Implement it.
  NOTIMPLEMENTED();
}

}  // namespace devtools_service
