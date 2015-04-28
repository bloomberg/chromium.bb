// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/dev_tools_manager_delegate_android.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/devtools_discovery/devtools_discovery_manager.h"
#include "components/history/core/browser/top_sites.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_target.h"

using content::DevToolsAgentHost;

DevToolsManagerDelegateAndroid::DevToolsManagerDelegateAndroid()
    : network_protocol_handler_(new DevToolsNetworkProtocolHandler()) {
}

DevToolsManagerDelegateAndroid::~DevToolsManagerDelegateAndroid() {
}

void DevToolsManagerDelegateAndroid::Inspect(
    content::BrowserContext* browser_context,
    content::DevToolsAgentHost* agent_host) {
}

base::DictionaryValue* DevToolsManagerDelegateAndroid::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command_dict) {
  return network_protocol_handler_->HandleCommand(agent_host, command_dict);
}

void DevToolsManagerDelegateAndroid::DevToolsAgentStateChanged(
    content::DevToolsAgentHost* agent_host,
    bool attached) {
  network_protocol_handler_->DevToolsAgentStateChanged(agent_host, attached);
}

scoped_ptr<content::DevToolsTarget>
    DevToolsManagerDelegateAndroid::CreateNewTarget(const GURL& url) {
  devtools_discovery::DevToolsDiscoveryManager* discovery_manager =
      devtools_discovery::DevToolsDiscoveryManager::GetInstance();
  return discovery_manager->CreateNew(url);
}

void DevToolsManagerDelegateAndroid::EnumerateTargets(TargetCallback callback) {
  TargetList targets;
  devtools_discovery::DevToolsDiscoveryManager* discovery_manager =
      devtools_discovery::DevToolsDiscoveryManager::GetInstance();
  for (const auto& descriptor : discovery_manager->GetDescriptors())
    targets.push_back(descriptor);
  callback.Run(targets);
}

std::string DevToolsManagerDelegateAndroid::GetPageThumbnailData(
    const GURL& url) {
  Profile* profile = ProfileManager::GetLastUsedProfile()->GetOriginalProfile();
  scoped_refptr<history::TopSites> top_sites =
      TopSitesFactory::GetForProfile(profile);
  if (top_sites) {
    scoped_refptr<base::RefCountedMemory> data;
    if (top_sites->GetPageThumbnail(url, false, &data))
      return std::string(data->front_as<char>(), data->size());
  }
  return std::string();
}
