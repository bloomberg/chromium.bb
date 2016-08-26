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

BasicTargetDescriptor::BasicTargetDescriptor(
    scoped_refptr<DevToolsAgentHost> agent_host)
    : agent_host_(agent_host) {
}

BasicTargetDescriptor::~BasicTargetDescriptor() {
}

scoped_refptr<DevToolsAgentHost> BasicTargetDescriptor::GetAgentHost() const {
  return agent_host_;
}

}  // namespace devtools_discovery
