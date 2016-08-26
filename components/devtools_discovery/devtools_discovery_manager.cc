// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_discovery/devtools_discovery_manager.h"

#include "base/stl_util.h"

using content::DevToolsAgentHost;

namespace devtools_discovery {

// static
DevToolsDiscoveryManager* DevToolsDiscoveryManager::GetInstance() {
  return base::Singleton<DevToolsDiscoveryManager>::get();
}

DevToolsDiscoveryManager::DevToolsDiscoveryManager() {
}

DevToolsDiscoveryManager::~DevToolsDiscoveryManager() {
  base::STLDeleteElements(&providers_);
}

void DevToolsDiscoveryManager::AddProvider(std::unique_ptr<Provider> provider) {
  providers_.push_back(provider.release());
}

content::DevToolsAgentHost::List DevToolsDiscoveryManager::GetDescriptors() {
  if (providers_.size())
    return GetDescriptorsFromProviders();

  return DevToolsAgentHost::GetOrCreateAll();
}

void DevToolsDiscoveryManager::SetCreateCallback(
    const CreateCallback& callback) {
  create_callback_ = callback;
}

scoped_refptr<content::DevToolsAgentHost> DevToolsDiscoveryManager::CreateNew(
    const GURL& url) {
  if (create_callback_.is_null())
    return nullptr;
  return create_callback_.Run(url);
}

content::DevToolsAgentHost::List
DevToolsDiscoveryManager::GetDescriptorsFromProviders() {
  content::DevToolsAgentHost::List result;
  for (auto* provider : providers_) {
    content::DevToolsAgentHost::List partial = provider->GetDescriptors();
    result.insert(result.begin(), partial.begin(), partial.end());
  }
  return result;
}

std::unique_ptr<base::DictionaryValue>
DevToolsDiscoveryManager::HandleCreateTargetCommand(
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
    scoped_refptr<content::DevToolsAgentHost> host = CreateNew(GURL(url));
    if (!host)
      return nullptr;
    std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
    result->SetInteger("id", id);
    std::unique_ptr<base::DictionaryValue> cmd_result(
        new base::DictionaryValue());
    cmd_result->SetString("targetId", host->GetId());
    result->Set("result", std::move(cmd_result));
    return result;
  }
  return nullptr;
}

}  // namespace devtools_discovery
