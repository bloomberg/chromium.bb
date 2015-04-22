// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_dev_tools_manager_delegate.h"

#include "components/devtools_discovery/devtools_discovery_manager.h"
#include "content/public/browser/devtools_target.h"

namespace android_webview {

AwDevToolsManagerDelegate::AwDevToolsManagerDelegate() {
}

AwDevToolsManagerDelegate::~AwDevToolsManagerDelegate() {
}

base::DictionaryValue* AwDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command_dict) {
  return NULL;
}

void AwDevToolsManagerDelegate::EnumerateTargets(TargetCallback callback) {
  TargetList targets;
  devtools_discovery::DevToolsDiscoveryManager* discovery_manager =
      devtools_discovery::DevToolsDiscoveryManager::GetInstance();
  for (const auto& descriptor : discovery_manager->GetDescriptors())
    targets.push_back(descriptor);
  callback.Run(targets);
}

std::string AwDevToolsManagerDelegate::GetPageThumbnailData(const GURL&) {
  return "";
}

scoped_ptr<content::DevToolsTarget> AwDevToolsManagerDelegate::CreateNewTarget(
    const GURL&) {
  return scoped_ptr<content::DevToolsTarget>();
}

}  // namespace android_webview
