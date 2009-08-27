// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/devtools_client.h"

#include "app/app_switches.h"
#include "base/command_line.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "webkit/glue/webdevtoolsclient.h"

DevToolsClient::DevToolsClient(RenderView* view)
    : render_view_(view) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  web_tools_client_.reset(
      WebDevToolsClient::Create(
          view->webview(),
          this,
          WideToASCII(command_line.GetSwitchValue(switches::kLang))));
}

DevToolsClient::~DevToolsClient() {
}

void DevToolsClient::Send(const IPC::Message& tools_agent_message) {
  render_view_->Send(new ViewHostMsg_ForwardToDevToolsAgent(
      render_view_->routing_id(),
      tools_agent_message));
}

bool DevToolsClient::OnMessageReceived(const IPC::Message& message) {
  DCHECK(RenderThread::current()->message_loop() == MessageLoop::current());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DevToolsClient, message)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_RpcMessage, OnRpcMessage)
    IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP()

  return handled;
}

void DevToolsClient::SendMessageToAgent(const std::string& class_name,
                                        const std::string& method_name,
                                        const std::string& param1,
                                        const std::string& param2,
                                        const std::string& param3) {
  Send(DevToolsAgentMsg_RpcMessage(class_name, method_name, param1, param2,
                                   param3));
}

void DevToolsClient::SendDebuggerCommandToAgent(const std::string& command) {
  Send(DevToolsAgentMsg_DebuggerCommand(command));
}

void DevToolsClient::ActivateWindow() {
  render_view_->Send(new ViewHostMsg_ActivateDevToolsWindow(
      render_view_->routing_id()));
}

void DevToolsClient::CloseWindow() {
  render_view_->Send(new ViewHostMsg_CloseDevToolsWindow(
      render_view_->routing_id()));
}

void DevToolsClient::DockWindow() {
  render_view_->Send(new ViewHostMsg_DockDevToolsWindow(
      render_view_->routing_id()));
}

void DevToolsClient::UndockWindow() {
  render_view_->Send(new ViewHostMsg_UndockDevToolsWindow(
      render_view_->routing_id()));
}

void DevToolsClient::ToggleInspectElementMode(bool enabled) {
  render_view_->Send(new ViewHostMsg_ToggleInspectElementMode(
      render_view_->routing_id(), enabled));
}


void DevToolsClient::OnRpcMessage(const std::string& class_name,
                                  const std::string& method_name,
                                  const std::string& param1,
                                  const std::string& param2,
                                  const std::string& param3) {
  web_tools_client_->DispatchMessageFromAgent(class_name, method_name, param1,
      param2, param3);
}
