// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_service/devtools_registry_impl.h"

#include <utility>

#include "base/logging.h"
#include "components/devtools_service/devtools_agent_host.h"

namespace devtools_service {

DevToolsRegistryImpl::Iterator::Iterator(DevToolsRegistryImpl* registry)
    : registry_(registry), iter_(registry->agents_.begin()) {
}

DevToolsRegistryImpl::Iterator::~Iterator() {
}

DevToolsRegistryImpl::DevToolsRegistryImpl(DevToolsService* service)
    : service_(service) {
}

DevToolsRegistryImpl::~DevToolsRegistryImpl() {
}

void DevToolsRegistryImpl::BindToRegistryRequest(
    mojo::InterfaceRequest<DevToolsRegistry> request) {
  bindings_.AddBinding(this, std::move(request));
}

DevToolsAgentHost* DevToolsRegistryImpl::GetAgentById(const std::string& id) {
  auto iter = agents_.find(id);
  if (iter == agents_.end())
    return nullptr;

  return iter->second.get();
}

void DevToolsRegistryImpl::RegisterAgent(const mojo::String& id,
                                         DevToolsAgentPtr agent) {
  linked_ptr<DevToolsAgentHost> agent_host(
      new DevToolsAgentHost(id, std::move(agent)));
  agent_host->set_agent_connection_error_handler(
      [this, id]() { OnAgentConnectionError(id); });

  agents_[id] = agent_host;
}

void DevToolsRegistryImpl::OnAgentConnectionError(const std::string& id) {
  DCHECK(agents_.find(id) != agents_.end());
  agents_.erase(id);
}

}  // namespace devtools_service
