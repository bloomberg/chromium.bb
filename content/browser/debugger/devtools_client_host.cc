// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "content/browser/debugger/devtools_client_host.h"
#include "content/browser/debugger/devtools_manager.h"

typedef std::vector<DevToolsClientHost*> DevToolsClientHostList;
namespace {
base::LazyInstance<DevToolsClientHostList,
                   base::LeakyLazyInstanceTraits<DevToolsClientHostList> >
     g_instances = LAZY_INSTANCE_INITIALIZER;
}  // namespace

// static
DevToolsClientHost* DevToolsClientHost::FindOwnerClientHost(
    RenderViewHost* client_rvh) {
  for (DevToolsClientHostList::iterator it = g_instances.Get().begin();
       it != g_instances.Get().end(); ++it) {
    if ((*it)->GetClientRenderViewHost() == client_rvh)
      return *it;
  }
  return NULL;
}

DevToolsClientHost::~DevToolsClientHost() {
  DevToolsClientHostList::iterator it = std::find(g_instances.Get().begin(),
                                                  g_instances.Get().end(),
                                                  this);
  DCHECK(it != g_instances.Get().end());
  g_instances.Get().erase(it);
}

RenderViewHost* DevToolsClientHost::GetClientRenderViewHost() {
  return NULL;
}

DevToolsClientHost::DevToolsClientHost() : close_listener_(NULL) {
  g_instances.Get().push_back(this);
}

void DevToolsClientHost::ForwardToDevToolsAgent(const IPC::Message& message) {
  DevToolsManager::GetInstance()->ForwardToDevToolsAgent(this, message);
}

void DevToolsClientHost::NotifyCloseListener() {
  if (close_listener_) {
    close_listener_->ClientHostClosing(this);
    close_listener_ = NULL;
  }
}
