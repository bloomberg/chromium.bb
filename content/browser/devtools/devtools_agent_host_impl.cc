// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_agent_host_impl.h"

#include <map>

#include "base/basictypes.h"
#include "base/guid.h"
#include "base/lazy_instance.h"

namespace content {

namespace {
typedef std::map<std::string, DevToolsAgentHostImpl*> Instances;
base::LazyInstance<Instances>::Leaky g_instances = LAZY_INSTANCE_INITIALIZER;
}  // namespace

DevToolsAgentHostImpl::DevToolsAgentHostImpl()
    : close_listener_(NULL),
      id_(base::GenerateGUID()) {
  g_instances.Get()[id_] = this;
}

DevToolsAgentHostImpl::~DevToolsAgentHostImpl() {
  g_instances.Get().erase(g_instances.Get().find(id_));
}

scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::GetForId(
    const std::string& id) {
  if (g_instances == NULL)
    return NULL;
  Instances::iterator it = g_instances.Get().find(id);
  if (it == g_instances.Get().end())
    return NULL;
  return it->second;
}

void DevToolsAgentHostImpl::InspectElement(int x, int y) {
}

std::string DevToolsAgentHostImpl::GetId() {
  return id_;
}

RenderViewHost* DevToolsAgentHostImpl::GetRenderViewHost() {
  return NULL;
}

void DevToolsAgentHostImpl::NotifyCloseListener() {
  if (close_listener_) {
    scoped_refptr<DevToolsAgentHostImpl> protect(this);
    close_listener_->AgentHostClosing(this);
    close_listener_ = NULL;
  }
}

}  // namespace content
