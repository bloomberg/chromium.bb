// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/debugger/devtools_frontend_message_handler.h"

#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/devtools_frontend_window.h"
#include "content/public/browser/devtools_frontend_window_delegate.h"

namespace content {

void SetupDevToolsFrontendDelegate(
    TabContents* client_tab_contents,
    DevToolsFrontendWindowDelegate* delegate) {
  new DevToolsFrontendMessageHandler(client_tab_contents, delegate);
}

DevToolsFrontendMessageHandler::DevToolsFrontendMessageHandler(
    TabContents* tab_contents,
    DevToolsFrontendWindowDelegate* delegate)
    : RenderViewHostObserver(tab_contents->render_view_host()),
      tab_contents_(tab_contents),
      delegate_(delegate) {
}

DevToolsFrontendMessageHandler::~DevToolsFrontendMessageHandler() {
}

bool DevToolsFrontendMessageHandler::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DevToolsFrontendMessageHandler, message)
    IPC_MESSAGE_HANDLER(DevToolsHostMsg_ForwardToAgent, ForwardToDevToolsAgent)
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

void DevToolsFrontendMessageHandler::ForwardToDevToolsAgent(
    const IPC::Message& message) {
  delegate_->ForwardToDevToolsAgent(message);
}

void DevToolsFrontendMessageHandler::OnActivateWindow() {
  delegate_->ActivateWindow();
}

void DevToolsFrontendMessageHandler::OnCloseWindow() {
  delegate_->CloseWindow();
}

void DevToolsFrontendMessageHandler::OnMoveWindow(int x, int y) {
  delegate_->MoveWindow(x, y);
}

void DevToolsFrontendMessageHandler::OnSaveAs(
    const std::string& suggested_file_name,
    const std::string& content) {
  delegate_->SaveToFile(suggested_file_name, content);
}

void DevToolsFrontendMessageHandler::OnRequestDockWindow() {
  delegate_->DockWindow();
}

void DevToolsFrontendMessageHandler::OnRequestUndockWindow() {
  delegate_->UndockWindow();
}

}  // namespace content
