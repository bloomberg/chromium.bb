// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/devtools/cast_dev_tools_delegate.h"

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/devtools_discovery/devtools_discovery_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_target.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "grit/shell_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromecast {
namespace shell {

// CastDevToolsDelegate -----------------------------------------------------

CastDevToolsDelegate::CastDevToolsDelegate() {
}

CastDevToolsDelegate::~CastDevToolsDelegate() {
}

std::string CastDevToolsDelegate::GetDiscoveryPageHTML() {
#if defined(OS_ANDROID)
  return std::string();
#else
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_CAST_SHELL_DEVTOOLS_DISCOVERY_PAGE).as_string();
#endif  // defined(OS_ANDROID)
}

std::string CastDevToolsDelegate::GetFrontendResource(
    const std::string& path) {
  return std::string();
}

// CastDevToolsManagerDelegate -----------------------------------------------

CastDevToolsManagerDelegate::CastDevToolsManagerDelegate() {
}

CastDevToolsManagerDelegate::~CastDevToolsManagerDelegate() {
}

base::DictionaryValue* CastDevToolsManagerDelegate::HandleCommand(
    content::DevToolsAgentHost* agent_host,
    base::DictionaryValue* command) {
  return NULL;
}

std::string CastDevToolsManagerDelegate::GetPageThumbnailData(
    const GURL& url) {
  return "";
}

scoped_ptr<content::DevToolsTarget>
CastDevToolsManagerDelegate::CreateNewTarget(const GURL& url) {
  return scoped_ptr<content::DevToolsTarget>();
}

void CastDevToolsManagerDelegate::EnumerateTargets(TargetCallback callback) {
  TargetList targets;
  devtools_discovery::DevToolsDiscoveryManager* discovery_manager =
      devtools_discovery::DevToolsDiscoveryManager::GetInstance();
  for (const auto& descriptor : discovery_manager->GetDescriptors())
    targets.push_back(descriptor);
  callback.Run(targets);
}

}  // namespace shell
}  // namespace chromecast
