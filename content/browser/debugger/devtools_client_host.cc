// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/devtools_client_host.h"

#include "content/browser/debugger/devtools_manager.h"

DevToolsClientHost::~DevToolsClientHost() {
}

DevToolsClientHost::DevToolsClientHost() : close_listener_(NULL) {
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
