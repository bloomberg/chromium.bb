// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/inspectable_tab_proxy.h"

#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/debugger/debugger_remote_service.h"
#include "chrome/browser/debugger/devtools_client_host.h"
#include "chrome/browser/sessions/session_id.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/common/devtools_messages.h"

// The debugged tab has closed.
void DevToolsClientHostImpl::InspectedTabClosing() {
  TabClosed();
  delete this;
}

// The remote debugger has detached.
void DevToolsClientHostImpl::Close() {
  NotifyCloseListener();
  delete this;
}

void DevToolsClientHostImpl::SendMessageToClient(
    const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(DevToolsClientHostImpl, msg)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_RpcMessage, OnRpcMessage);
    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()
}

void DevToolsClientHostImpl::OnRpcMessage(const DevToolsMessageData& data) {
  static const std::string kDebuggerAgentDelegate = "DebuggerAgentDelegate";
  static const std::string kToolsAgentDelegate = "ToolsAgentDelegate";
  static const std::string kDebuggerOutput = "debuggerOutput";
  static const std::string kFrameNavigate = "frameNavigate";

  if (data.class_name == kDebuggerAgentDelegate &&
      data.method_name == kDebuggerOutput) {
    DebuggerOutput(data.arguments[0]);
  } else if (data.class_name == kToolsAgentDelegate &&
             data.method_name == kFrameNavigate) {
    FrameNavigate(data.arguments[0]);
  }
}

void DevToolsClientHostImpl::DebuggerOutput(const std::string& msg) {
  service_->DebuggerOutput(id_, msg);
}

void DevToolsClientHostImpl::FrameNavigate(const std::string& url) {
  service_->FrameNavigate(id_, url);
}

void DevToolsClientHostImpl::TabClosed() {
  service_->TabClosed(id_);
}

const InspectableTabProxy::ControllersMap&
    InspectableTabProxy::controllers_map() {
  controllers_map_.clear();
  for (BrowserList::const_iterator it = BrowserList::begin(),
       end = BrowserList::end(); it != end; ++it) {
    TabStripModel* model = (*it)->tabstrip_model();
    for (int i = 0, size = model->count(); i < size; ++i) {
      NavigationController& controller =
          model->GetTabContentsAt(i)->controller();
      controllers_map_[controller.session_id().id()] = &controller;
    }
  }
  return controllers_map_;
}

DevToolsClientHostImpl* InspectableTabProxy::ClientHostForTabId(
    int32 id) {
  InspectableTabProxy::IdToClientHostMap::const_iterator it =
      id_to_client_host_map_.find(id);
  if (it == id_to_client_host_map_.end()) {
    return NULL;
  }
  return it->second;
}

DevToolsClientHost* InspectableTabProxy::NewClientHost(
    int32 id,
    DebuggerRemoteService* service) {
  DevToolsClientHostImpl* client_host =
      new DevToolsClientHostImpl(id, service, &id_to_client_host_map_);
  id_to_client_host_map_[id] = client_host;
  return client_host;
}

void InspectableTabProxy::OnRemoteDebuggerDetached() {
  while (id_to_client_host_map_.size() > 0) {
    IdToClientHostMap::iterator it = id_to_client_host_map_.begin();
    it->second->debugger_remote_service()->DetachFromTab(IntToString(it->first),
                                                         NULL);
  }
}
