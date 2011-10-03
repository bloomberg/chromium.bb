// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/devtools_client.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "content/common/content_switches.h"
#include "content/common/devtools_messages.h"
#include "content/renderer/render_thread.h"
#include "content/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDevToolsFrontend.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "ui/base/ui_base_switches.h"

using WebKit::WebDevToolsFrontend;
using WebKit::WebString;

DevToolsClient::DevToolsClient(RenderView* render_view)
    : content::RenderViewObserver(render_view) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  web_tools_frontend_.reset(
      WebDevToolsFrontend::create(
          render_view->webview(),
          this,
          ASCIIToUTF16(command_line.GetSwitchValueASCII(switches::kLang))));
}

DevToolsClient::~DevToolsClient() {
}

void DevToolsClient::SendToAgent(const IPC::Message& tools_agent_message) {
  Send(new DevToolsHostMsg_ForwardToAgent(routing_id(), tools_agent_message));
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
  SendToAgent(DevToolsAgentMsg_FrontendLoaded(MSG_ROUTING_NONE));
}

void DevToolsClient::sendMessageToBackend(const WebString& message)  {
  SendToAgent(DevToolsAgentMsg_DispatchOnInspectorBackend(MSG_ROUTING_NONE,
                                                          message.utf8()));
}

void DevToolsClient::sendDebuggerCommandToAgent(const WebString& command) {
  SendToAgent(DevToolsAgentMsg_DebuggerCommand(MSG_ROUTING_NONE,
                                               command.utf8()));
}

void DevToolsClient::activateWindow() {
  Send(new DevToolsHostMsg_ActivateWindow(routing_id()));
}

void DevToolsClient::closeWindow() {
  Send(new DevToolsHostMsg_CloseWindow(routing_id()));
}

void DevToolsClient::requestDockWindow() {
  Send(new DevToolsHostMsg_RequestDockWindow(routing_id()));
}

void DevToolsClient::requestUndockWindow() {
  Send(new DevToolsHostMsg_RequestUndockWindow(routing_id()));
}

void DevToolsClient::saveAs(const WebKit::WebString& file_name,
                            const WebKit::WebString& content) {
  Send(new DevToolsHostMsg_SaveAs(routing_id(),
                                  file_name.utf8(),
                                  content.utf8()));
}

void DevToolsClient::OnDispatchOnInspectorFrontend(const std::string& message) {
  web_tools_frontend_->dispatchOnInspectorFrontend(
      WebString::fromUTF8(message));
}

bool DevToolsClient::shouldHideScriptsPanel() {
  CommandLine* cmd = CommandLine::ForCurrentProcess();
  return cmd->HasSwitch(switches::kRemoteShellPort);
}
