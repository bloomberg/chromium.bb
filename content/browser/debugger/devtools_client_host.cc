// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/logging.h"
#include "content/browser/debugger/devtools_client_host.h"
#include "content/browser/debugger/devtools_manager.h"

DevToolsClientHost::DevToolsClientHostList DevToolsClientHost::instances_;

// static
DevToolsClientHost* DevToolsClientHost::FindOwnerClientHost(
    RenderViewHost* client_rvh) {
  for (DevToolsClientHostList::iterator it = instances_.begin();
       it != instances_.end(); ++it) {
    if ((*it)->GetClientRenderViewHost() == client_rvh)
      return *it;
  }
  return NULL;
}

DevToolsClientHost::~DevToolsClientHost() {
  DevToolsClientHostList::iterator it = std::find(instances_.begin(),
                                                  instances_.end(),
                                                  this);
  DCHECK(it != instances_.end());
  instances_.erase(it);
}

RenderViewHost* DevToolsClientHost::GetClientRenderViewHost() {
  return NULL;
}

DevToolsClientHost::DevToolsClientHost() : close_listener_(NULL) {
  instances_.push_back(this);
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
