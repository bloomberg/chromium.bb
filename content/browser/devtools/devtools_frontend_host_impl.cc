// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_frontend_host_impl.h"

#include "content/common/devtools_messages.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace content {

// static
DevToolsFrontendHost* DevToolsFrontendHost::Create(
    RenderViewHost* frontend_rvh,
    DevToolsFrontendHost::Delegate* delegate) {
  return new DevToolsFrontendHostImpl(frontend_rvh, delegate);
}

DevToolsFrontendHostImpl::DevToolsFrontendHostImpl(
    RenderViewHost* frontend_rvh,
    DevToolsFrontendHost::Delegate* delegate)
    : WebContentsObserver(WebContents::FromRenderViewHost(frontend_rvh)),
      delegate_(delegate) {
  frontend_rvh->Send(new DevToolsMsg_SetupDevToolsClient(
      frontend_rvh->GetRoutingID()));
}

DevToolsFrontendHostImpl::~DevToolsFrontendHostImpl() {
}

void DevToolsFrontendHostImpl::DispatchOnDevToolsFrontend(
    const std::string& message) {
  if (!web_contents())
    return;
  RenderViewHost* target_host = web_contents()->GetRenderViewHost();
  target_host->Send(new DevToolsClientMsg_DispatchOnInspectorFrontend(
      target_host->GetRoutingID(),
      message));
}

bool DevToolsFrontendHostImpl::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DevToolsFrontendHostImpl, message)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DispatchOnInspectorBackend,
                        OnDispatchOnInspectorBackend)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_DispatchOnEmbedder,
                        OnDispatchOnEmbedder)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DevToolsFrontendHostImpl::OnDispatchOnInspectorBackend(
    const std::string& message) {
  delegate_->HandleMessageFromDevToolsFrontendToBackend(message);
}

void DevToolsFrontendHostImpl::OnDispatchOnEmbedder(
    const std::string& message) {
  delegate_->HandleMessageFromDevToolsFrontend(message);
}

}  // namespace content
