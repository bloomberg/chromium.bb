// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_discovery/devtools_discovery_manager.h"

#include "base/stl_util.h"
#include "components/devtools_discovery/basic_target_descriptor.h"
#include "content/public/browser/devtools_agent_host.h"

using content::DevToolsAgentHost;

namespace devtools_discovery {

// static
DevToolsDiscoveryManager* DevToolsDiscoveryManager::GetInstance() {
  return base::Singleton<DevToolsDiscoveryManager>::get();
}

DevToolsDiscoveryManager::DevToolsDiscoveryManager() {
}

DevToolsDiscoveryManager::~DevToolsDiscoveryManager() {
  STLDeleteElements(&providers_);
}

void DevToolsDiscoveryManager::AddProvider(std::unique_ptr<Provider> provider) {
  providers_.push_back(provider.release());
}

DevToolsTargetDescriptor::List DevToolsDiscoveryManager::GetDescriptors() {
  if (providers_.size())
    return GetDescriptorsFromProviders();

  DevToolsAgentHost::List agent_hosts = DevToolsAgentHost::GetOrCreateAll();
  DevToolsTargetDescriptor::List result;
  result.reserve(agent_hosts.size());
  for (const auto& agent_host : agent_hosts)
    result.push_back(new BasicTargetDescriptor(agent_host));
  return result;
}

void DevToolsDiscoveryManager::SetCreateCallback(
    const CreateCallback& callback) {
  create_callback_ = callback;
}

std::unique_ptr<DevToolsTargetDescriptor> DevToolsDiscoveryManager::CreateNew(
    const GURL& url) {
  if (create_callback_.is_null())
    return nullptr;
  return create_callback_.Run(url);
}

DevToolsTargetDescriptor::List
DevToolsDiscoveryManager::GetDescriptorsFromProviders() {
  DevToolsTargetDescriptor::List result;
  for (auto* provider : providers_) {
    DevToolsTargetDescriptor::List partial = provider->GetDescriptors();
    result.insert(result.begin(), partial.begin(), partial.end());
  }
  return result;
}

std::unique_ptr<base::DictionaryValue>
DevToolsDiscoveryManager::HandleNewTargetCommand(
    base::DictionaryValue* command_dict) {
  int id;
  std::string method;
  std::string url;
  const base::DictionaryValue* params_dict = nullptr;
  if (command_dict->GetInteger("id", &id) &&
      command_dict->GetString("method", &method) &&
      method == "Browser.createTarget" &&
      command_dict->GetDictionary("params", &params_dict) &&
      params_dict->GetString("url", &url)) {
    std::unique_ptr<devtools_discovery::DevToolsTargetDescriptor> descriptor =
        CreateNew(GURL(url));
    if (!descriptor)
      return nullptr;
    std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
    result->SetInteger("id", id);
    std::unique_ptr<base::DictionaryValue> cmd_result(
        new base::DictionaryValue());
    cmd_result->SetString("pageId", descriptor->GetId());
    result->Set("result", std::move(cmd_result));
    return result;
  }
  return nullptr;
}

}  // namespace devtools_discovery
