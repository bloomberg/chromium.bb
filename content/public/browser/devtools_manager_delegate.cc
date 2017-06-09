// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_manager_delegate.h"

namespace content {

void DevToolsManagerDelegate::Inspect(DevToolsAgentHost* agent_host) {
}

std::string DevToolsManagerDelegate::GetTargetType(WebContents* wc) {
  return std::string();
}

std::string DevToolsManagerDelegate::GetTargetTitle(WebContents* wc) {
  return std::string();
}

std::string DevToolsManagerDelegate::GetTargetDescription(WebContents* wc) {
  return std::string();
}

DevToolsAgentHost::List DevToolsManagerDelegate::RemoteDebuggingTargets() {
  return DevToolsAgentHost::GetOrCreateAll();
}

scoped_refptr<DevToolsAgentHost> DevToolsManagerDelegate::CreateNewTarget(
    const GURL& url) {
  return nullptr;
}

base::DictionaryValue* DevToolsManagerDelegate::HandleCommand(
      DevToolsAgentHost* agent_host,
      base::DictionaryValue* command) {
  return nullptr;
}

bool DevToolsManagerDelegate::HandleAsyncCommand(
    DevToolsAgentHost* agent_host,
    base::DictionaryValue* command,
    const CommandCallback& callback) {
  return false;
}

std::string DevToolsManagerDelegate::GetDiscoveryPageHTML() {
  return std::string();
}

std::string DevToolsManagerDelegate::GetFrontendResource(
    const std::string& path) {
  return std::string();
}

DevToolsManagerDelegate::~DevToolsManagerDelegate() {
}

}  // namespace content
