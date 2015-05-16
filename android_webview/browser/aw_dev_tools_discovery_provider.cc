// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_dev_tools_discovery_provider.h"

#include "android_webview/browser/browser_view_renderer.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/devtools_discovery/basic_target_descriptor.h"
#include "components/devtools_discovery/devtools_discovery_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/web_contents.h"

using content::DevToolsAgentHost;
using content::WebContents;

namespace {

std::string GetViewDescription(WebContents* web_contents) {
  android_webview::BrowserViewRenderer* bvr =
      android_webview::BrowserViewRenderer::FromWebContents(web_contents);
  if (!bvr) return "";
  base::DictionaryValue description;
  description.SetBoolean("attached", bvr->attached_to_window());
  description.SetBoolean("visible", bvr->IsVisible());
  gfx::Rect screen_rect = bvr->GetScreenRect();
  description.SetInteger("screenX", screen_rect.x());
  description.SetInteger("screenY", screen_rect.y());
  description.SetBoolean("empty", screen_rect.size().IsEmpty());
  if (!screen_rect.size().IsEmpty()) {
    description.SetInteger("width", screen_rect.width());
    description.SetInteger("height", screen_rect.height());
  }
  std::string json;
  base::JSONWriter::Write(description, &json);
  return json;
}

class TargetDescriptor : public devtools_discovery::BasicTargetDescriptor {
 public:
  explicit TargetDescriptor(scoped_refptr<DevToolsAgentHost> agent_host);

  // devtools_discovery::BasicTargetDescriptor overrides.
  std::string GetDescription() const override { return description_; }

 private:
  std::string description_;

  DISALLOW_COPY_AND_ASSIGN(TargetDescriptor);
};

TargetDescriptor::TargetDescriptor(scoped_refptr<DevToolsAgentHost> agent_host)
    : BasicTargetDescriptor(agent_host) {
  if (WebContents* web_contents = agent_host->GetWebContents())
    description_ = GetViewDescription(web_contents);
}

}  // namespace

namespace android_webview {

// static
void AwDevToolsDiscoveryProvider::Install() {
  devtools_discovery::DevToolsDiscoveryManager* discovery_manager =
      devtools_discovery::DevToolsDiscoveryManager::GetInstance();
  discovery_manager->AddProvider(
      make_scoped_ptr(new AwDevToolsDiscoveryProvider()));
}

AwDevToolsDiscoveryProvider::AwDevToolsDiscoveryProvider() {
}

AwDevToolsDiscoveryProvider::~AwDevToolsDiscoveryProvider() {
}

devtools_discovery::DevToolsTargetDescriptor::List
AwDevToolsDiscoveryProvider::GetDescriptors() {
  DevToolsAgentHost::List agent_hosts = DevToolsAgentHost::GetOrCreateAll();
  devtools_discovery::DevToolsTargetDescriptor::List result;
  result.reserve(agent_hosts.size());
  for (const auto& agent_host : agent_hosts)
    result.push_back(new TargetDescriptor(agent_host));
  return result;
}

}  // namespace android_webview
