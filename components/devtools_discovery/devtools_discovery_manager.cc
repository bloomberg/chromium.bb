// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_discovery/devtools_discovery_manager.h"

#include "components/devtools_discovery/basic_target_descriptor.h"
#include "content/public/browser/devtools_agent_host.h"

using content::DevToolsAgentHost;

namespace devtools_discovery {

// static
DevToolsDiscoveryManager* DevToolsDiscoveryManager::GetInstance() {
  return Singleton<DevToolsDiscoveryManager>::get();
}

DevToolsDiscoveryManager::DevToolsDiscoveryManager() {
}

DevToolsDiscoveryManager::~DevToolsDiscoveryManager() {
}

DevToolsTargetDescriptor::List DevToolsDiscoveryManager::GetDescriptors() {
  DevToolsAgentHost::List agent_hosts = DevToolsAgentHost::GetOrCreateAll();
  DevToolsTargetDescriptor::List result;
  result.reserve(agent_hosts.size());
  for (const auto& agent_host : agent_hosts)
    result.push_back(new BasicTargetDescriptor(agent_host));
  return result;
}

}  // namespace devtools_discovery
