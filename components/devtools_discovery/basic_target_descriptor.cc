// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_discovery/basic_target_descriptor.h"

#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"

using content::DevToolsAgentHost;

namespace devtools_discovery {

const char BasicTargetDescriptor::kTypePage[] = "page";
const char BasicTargetDescriptor::kTypeServiceWorker[] = "service_worker";
const char BasicTargetDescriptor::kTypeSharedWorker[] = "worker";
const char BasicTargetDescriptor::kTypeOther[] = "other";

BasicTargetDescriptor::BasicTargetDescriptor(
    scoped_refptr<DevToolsAgentHost> agent_host)
    : agent_host_(agent_host) {
  if (content::WebContents* web_contents = agent_host_->GetWebContents()) {
    content::NavigationController& controller = web_contents->GetController();
    content::NavigationEntry* entry = controller.GetActiveEntry();
    if (entry != NULL && entry->GetURL().is_valid())
      favicon_url_ = entry->GetFavicon().url;
    last_activity_time_ = web_contents->GetLastActiveTime();
  }
}

BasicTargetDescriptor::~BasicTargetDescriptor() {
}

std::string BasicTargetDescriptor::GetId() const {
  return agent_host_->GetId();
}

std::string BasicTargetDescriptor::GetParentId() const {
  return std::string();
}

std::string BasicTargetDescriptor::GetType() const {
  switch (agent_host_->GetType()) {
    case DevToolsAgentHost::TYPE_WEB_CONTENTS:
      return kTypePage;
    case DevToolsAgentHost::TYPE_SERVICE_WORKER:
      return kTypeServiceWorker;
    case DevToolsAgentHost::TYPE_SHARED_WORKER:
      return kTypeSharedWorker;
    default:
      break;
  }
  return kTypeOther;
}

std::string BasicTargetDescriptor::GetTitle() const {
  return agent_host_->GetTitle();
}

std::string BasicTargetDescriptor::GetDescription() const {
  return std::string();
}

GURL BasicTargetDescriptor::GetURL() const {
  return agent_host_->GetURL();
}

GURL BasicTargetDescriptor::GetFaviconURL() const {
  return favicon_url_;
}

base::TimeTicks BasicTargetDescriptor::GetLastActivityTime() const {
  return last_activity_time_;
}

bool BasicTargetDescriptor::IsAttached() const {
  return agent_host_->IsAttached();
}

scoped_refptr<DevToolsAgentHost> BasicTargetDescriptor::GetAgentHost() const {
  return agent_host_;
}

bool BasicTargetDescriptor::Activate() const {
  return agent_host_->Activate();
}

bool BasicTargetDescriptor::Close() const {
  return agent_host_->Close();
}

}  // namespace devtools_discovery
