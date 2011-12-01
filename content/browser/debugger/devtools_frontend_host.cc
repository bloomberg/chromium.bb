// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/devtools_frontend_host.h"

#include "content/browser/debugger/devtools_manager_impl.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_frontend_host_delegate.h"

namespace content {

// static
DevToolsClientHost* DevToolsClientHost::CreateDevToolsFrontendHost(
    TabContents* client_tab_contents,
    DevToolsFrontendHostDelegate* delegate) {
  return new DevToolsFrontendHost(client_tab_contents, delegate);
}

// static
void DevToolsClientHost::SetupDevToolsFrontendClient(
    RenderViewHost* frontend_rvh) {
  frontend_rvh->Send(new DevToolsMsg_SetupDevToolsClient(
      frontend_rvh->routing_id()));
}

DevToolsFrontendHost::DevToolsFrontendHost(
    TabContents* tab_contents,
    DevToolsFrontendHostDelegate* delegate)
    : RenderViewHostObserver(tab_contents->render_view_host()),
      tab_contents_(tab_contents),
      delegate_(delegate) {
}

DevToolsFrontendHost::~DevToolsFrontendHost() {
}

void DevToolsFrontendHost::DispatchOnInspectorFrontend(
    const std::string& message) {
  RenderViewHost* target_host = tab_contents_->render_view_host();
  target_host->Send(new DevToolsClientMsg_DispatchOnInspectorFrontend(
      target_host->routing_id(),
      message));
}

void DevToolsFrontendHost::InspectedTabClosing() {
  delegate_->InspectedTabClosing();
}

void DevToolsFrontendHost::FrameNavigating(const std::string& url) {
  delegate_->FrameNavigating(url);
}

void DevToolsFrontendHost::TabReplaced(TabContents* new_tab) {
  delegate_->TabReplaced(new_tab);
}

bool DevToolsFrontendHost::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DevToolsFrontendHost, message)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DispatchOnInspectorBackend,
                        OnDispatchOnInspectorBackend)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_ActivateWindow, OnActivateWindow)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_CloseWindow, OnCloseWindow)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_MoveWindow, OnMoveWindow)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_RequestDockWindow, OnRequestDockWindow)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_RequestUndockWindow,
                        OnRequestUndockWindow)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_SaveAs,
                        OnSaveAs)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DevToolsFrontendHost::OnDispatchOnInspectorBackend(
    const std::string& message) {
  DevToolsManagerImpl::GetInstance()->DispatchOnInspectorBackend(this, message);
//  delegate_->DispatchOnInspectorBackend(message);
}

void DevToolsFrontendHost::OnActivateWindow() {
  delegate_->ActivateWindow();
}

void DevToolsFrontendHost::OnCloseWindow() {
  delegate_->CloseWindow();
}

void DevToolsFrontendHost::OnMoveWindow(int x, int y) {
  delegate_->MoveWindow(x, y);
}

void DevToolsFrontendHost::OnSaveAs(
    const std::string& suggested_file_name,
    const std::string& content) {
  delegate_->SaveToFile(suggested_file_name, content);
}

void DevToolsFrontendHost::OnRequestDockWindow() {
  delegate_->DockWindow();
}

void DevToolsFrontendHost::OnRequestUndockWindow() {
  delegate_->UndockWindow();
}

}  // namespace content
