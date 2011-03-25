// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/debugger/devtools_tab_helper.h"

#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/common/devtools_messages.h"
#include "content/browser/tab_contents/tab_contents.h"

DevToolsTabHelper::DevToolsTabHelper(TabContents* tab_contents)
    : TabContentsObserver(tab_contents) {
}

DevToolsTabHelper::~DevToolsTabHelper() {
}

bool DevToolsTabHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DevToolsTabHelper, message)
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

void DevToolsTabHelper::OnForwardToAgent(const IPC::Message& message) {
  DevToolsManager::GetInstance()->ForwardToDevToolsAgent(
      tab_contents()->render_view_host(), message);
}

void DevToolsTabHelper::OnForwardToClient(const IPC::Message& message) {
  DevToolsManager::GetInstance()->ForwardToDevToolsClient(
      tab_contents()->render_view_host(), message);
}

void DevToolsTabHelper::OnActivateWindow() {
  DevToolsManager::GetInstance()->ActivateWindow(
      tab_contents()->render_view_host());
}

void DevToolsTabHelper::OnCloseWindow() {
  DevToolsManager::GetInstance()->CloseWindow(
      tab_contents()->render_view_host());
}

void DevToolsTabHelper::OnRequestDockWindow() {
  DevToolsManager::GetInstance()->RequestDockWindow(
      tab_contents()->render_view_host());
}

void DevToolsTabHelper::OnRequestUndockWindow() {
  DevToolsManager::GetInstance()->RequestUndockWindow(
      tab_contents()->render_view_host());
}

void DevToolsTabHelper::OnRuntimePropertyChanged(
    const std::string& name,
    const std::string& value) {
  DevToolsManager::GetInstance()->RuntimePropertyChanged(
      tab_contents()->render_view_host(), name, value);
}
