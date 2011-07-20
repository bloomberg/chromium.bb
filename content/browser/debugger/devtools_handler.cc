// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/devtools_handler.h"

#include "content/browser/content_browser_client.h"
#include "content/browser/debugger/devtools_client_host.h"
#include "content/browser/debugger/devtools_manager.h"
#include "content/browser/debugger/worker_devtools_manager_io.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/devtools_messages.h"

DevToolsHandler::DevToolsHandler(RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host) {
}

DevToolsHandler::~DevToolsHandler() {
}

bool DevToolsHandler::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DevToolsHandler, message)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_ForwardToAgent, OnForwardToAgent)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_ForwardToClient, OnForwardToClient)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_ActivateWindow, OnActivateWindow)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_CloseWindow, OnCloseWindow)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_RequestDockWindow, OnRequestDockWindow)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_RequestUndockWindow,
                        OnRequestUndockWindow)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_SaveAs,
                        OnSaveAs)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_RuntimePropertyChanged,
                        OnRuntimePropertyChanged)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_ClearBrowserCache, OnClearBrowserCache)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_ClearBrowserCookies,
                        OnClearBrowserCookies)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DevToolsHandler::OnForwardToAgent(const IPC::Message& message) {
  DevToolsClientHost* client_host = GetOwnerClientHost();
  if (!client_host)
    return;
  if (DevToolsManager::GetInstance()->
          ForwardToDevToolsAgent(client_host, message))
    return;
  WorkerDevToolsManagerIO::ForwardToWorkerDevToolsAgentOnUIThread(
      client_host, message);
}

void DevToolsHandler::OnForwardToClient(const IPC::Message& message) {
  DevToolsManager::GetInstance()->ForwardToDevToolsClient(
      render_view_host(), message);
}

void DevToolsHandler::OnActivateWindow() {
  DevToolsClientHost* client_host = GetOwnerClientHost();
  if (client_host)
    client_host->Activate();
}

void DevToolsHandler::OnCloseWindow() {
  DevToolsClientHost* client_host = GetOwnerClientHost();
  if (client_host)
    client_host->Close();
}

void DevToolsHandler::OnRequestDockWindow() {
  DevToolsClientHost* client_host = GetOwnerClientHost();
  if (client_host)
    client_host->SetDocked(true);
}

void DevToolsHandler::OnRequestUndockWindow() {
  DevToolsClientHost* client_host = GetOwnerClientHost();
  if (client_host)
    client_host->SetDocked(false);
}

void DevToolsHandler::OnSaveAs(const std::string& file_name,
                               const std::string& content) {
  DevToolsClientHost* client_host = GetOwnerClientHost();
  if (client_host)
    client_host->SaveAs(file_name, content);
}

void DevToolsHandler::OnRuntimePropertyChanged(const std::string& name,
                                               const std::string& value) {
  DevToolsManager::GetInstance()->RuntimePropertyChanged(
      render_view_host(), name, value);
}

void DevToolsHandler::OnClearBrowserCache() {
  content::GetContentClient()->browser()->ClearCache(render_view_host());
}

void DevToolsHandler::OnClearBrowserCookies() {
  content::GetContentClient()->browser()->ClearCookies(render_view_host());
}

DevToolsClientHost* DevToolsHandler::GetOwnerClientHost() {
  return DevToolsClientHost::FindOwnerClientHost(render_view_host());
}
