// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_handler.h"

#include "chrome/browser/debugger/devtools_file_util.h"
#include "chrome/browser/debugger/devtools_manager.h"
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
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DevToolsHandler::OnForwardToAgent(const IPC::Message& message) {
  DevToolsManager::GetInstance()->ForwardToDevToolsAgent(
      render_view_host(), message);
}

void DevToolsHandler::OnForwardToClient(const IPC::Message& message) {
  DevToolsManager::GetInstance()->ForwardToDevToolsClient(
      render_view_host(), message);
}

void DevToolsHandler::OnActivateWindow() {
  DevToolsManager::GetInstance()->ActivateWindow(render_view_host());
}

void DevToolsHandler::OnCloseWindow() {
  DevToolsManager::GetInstance()->CloseWindow(render_view_host());
}

void DevToolsHandler::OnRequestDockWindow() {
  DevToolsManager::GetInstance()->RequestDockWindow(render_view_host());
}

void DevToolsHandler::OnRequestUndockWindow() {
  DevToolsManager::GetInstance()->RequestUndockWindow(render_view_host());
}

void DevToolsHandler::OnSaveAs(const std::string& file_name,
                               const std::string& content) {
  TabContents* tab_contents = render_view_host()->delegate() ?
      render_view_host()->delegate()->GetAsTabContents() : NULL;
  DCHECK(tab_contents);
  if (!tab_contents)
    return;

  DevToolsFileUtil::SaveAs(tab_contents->profile(), file_name, content);
}

void DevToolsHandler::OnRuntimePropertyChanged(const std::string& name,
                                               const std::string& value) {
  DevToolsManager::GetInstance()->RuntimePropertyChanged(
      render_view_host(), name, value);
}
