// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/devtools_client.h"

#include "app/app_switches.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/devtools_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDevToolsFrontend.h"
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
          ASCIIToUTF16(command_line.GetSwitchValueASCII(switches::kLang))));
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
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                        OnDispatchOnInspectorFrontend)
    IPC_MESSAGE_UNHANDLED(handled = false);
  IPC_END_MESSAGE_MAP()

  return handled;
}

void DevToolsClient::sendFrontendLoaded() {
  Send(DevToolsAgentMsg_FrontendLoaded());
}

void DevToolsClient::sendMessageToBackend(const WebString& message)  {
  Send(DevToolsAgentMsg_DispatchOnInspectorBackend(message.utf8()));
}

void DevToolsClient::sendDebuggerCommandToAgent(const WebString& command) {
  Send(DevToolsAgentMsg_DebuggerCommand(command.utf8()));
}

void DevToolsClient::activateWindow() {
  render_view_->Send(new ViewHostMsg_ActivateDevToolsWindow(
      render_view_->routing_id()));
}

void DevToolsClient::closeWindow() {
  render_view_->Send(new ViewHostMsg_CloseDevToolsWindow(
      render_view_->routing_id()));
}

void DevToolsClient::requestDockWindow() {
  render_view_->Send(new ViewHostMsg_RequestDockDevToolsWindow(
      render_view_->routing_id()));
}

void DevToolsClient::requestUndockWindow() {
  render_view_->Send(new ViewHostMsg_RequestUndockDevToolsWindow(
      render_view_->routing_id()));
}

void DevToolsClient::OnDispatchOnInspectorFrontend(
    const std::string& message) {
      web_tools_frontend_->dispatchOnInspectorFrontend(
          WebString::fromUTF8(message));
}

bool DevToolsClient::shouldHideScriptsPanel() {
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  return cmd->HasSwitch(switches::kRemoteShellPort);
}
