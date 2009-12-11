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
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsFrontend.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsMessageData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"

using WebKit::WebDevToolsFrontend;
using WebKit::WebString;

DevToolsClient::DevToolsClient(RenderView* view)
    : render_view_(view) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  web_tools_frontend_.reset(
      WebDevToolsFrontend::create(
          view->webview(),
          this,
          WideToUTF16Hack(command_line.GetSwitchValue(switches::kLang))));
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

void DevToolsClient::sendMessageToAgent(
      const WebKit::WebDevToolsMessageData& data) {
  Send(DevToolsAgentMsg_RpcMessage(DevToolsMessageData(data)));
}

void DevToolsClient::sendDebuggerCommandToAgent(const WebString& command) {
  Send(DevToolsAgentMsg_DebuggerCommand(command.utf8()));
}

void DevToolsClient::sendDebuggerPauseScript() {
  Send(DevToolsAgentMsg_DebuggerPauseScript());
}

void DevToolsClient::activateWindow() {
  render_view_->Send(new ViewHostMsg_ActivateDevToolsWindow(
      render_view_->routing_id()));
}

void DevToolsClient::closeWindow() {
  render_view_->Send(new ViewHostMsg_CloseDevToolsWindow(
      render_view_->routing_id()));
}

void DevToolsClient::dockWindow() {
  render_view_->Send(new ViewHostMsg_DockDevToolsWindow(
      render_view_->routing_id()));
}

void DevToolsClient::undockWindow() {
  render_view_->Send(new ViewHostMsg_UndockDevToolsWindow(
      render_view_->routing_id()));
}

void DevToolsClient::OnRpcMessage(const DevToolsMessageData& data) {
  web_tools_frontend_->dispatchMessageFromAgent(
      data.ToWebDevToolsMessageData());
}
