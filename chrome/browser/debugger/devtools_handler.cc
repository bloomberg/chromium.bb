// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_handler.h"

#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/common/devtools_messages.h"
#include "content/browser/tab_contents/tab_contents.h"

DevToolsHandler::DevToolsHandler(TabContents* tab,
                                 RenderViewHost* render_view_host)
    : tab_(tab), render_view_host_(render_view_host) {
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
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_RuntimePropertyChanged,
                        OnRuntimePropertyChanged)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DevToolsHandler::OnForwardToAgent(const IPC::Message& message) {
  DevToolsManager::GetInstance()->ForwardToDevToolsAgent(
      GetRenderViewHost(), message);
}

void DevToolsHandler::OnForwardToClient(const IPC::Message& message) {
  DevToolsManager::GetInstance()->ForwardToDevToolsClient(
      GetRenderViewHost(), message);
}

void DevToolsHandler::OnActivateWindow() {
  DevToolsManager::GetInstance()->ActivateWindow(GetRenderViewHost());
}

void DevToolsHandler::OnCloseWindow() {
  DevToolsManager::GetInstance()->CloseWindow(GetRenderViewHost());
}

void DevToolsHandler::OnRequestDockWindow() {
  DevToolsManager::GetInstance()->RequestDockWindow(GetRenderViewHost());
}

void DevToolsHandler::OnRequestUndockWindow() {
  DevToolsManager::GetInstance()->RequestUndockWindow(GetRenderViewHost());
}

void DevToolsHandler::OnRuntimePropertyChanged(const std::string& name,
                                               const std::string& value) {
  DevToolsManager::GetInstance()->RuntimePropertyChanged(
      GetRenderViewHost(), name, value);
}

RenderViewHost* DevToolsHandler::GetRenderViewHost() {
  return tab_ ? tab_->render_view_host() : render_view_host_;
}


DevToolsTabHelper::DevToolsTabHelper(TabContents* tab_contents)
    : TabContentsObserver(tab_contents),
      handler_(tab_contents, NULL) {
}

bool DevToolsTabHelper::OnMessageReceived(const IPC::Message& message) {
  return handler_.OnMessageReceived(message);
}
