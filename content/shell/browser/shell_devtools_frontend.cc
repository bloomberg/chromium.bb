// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_devtools_frontend.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"
#include "content/shell/browser/webkit_test_controller.h"
#include "content/shell/common/shell_switches.h"
#include "net/base/filename_util.h"

namespace content {

// This constant should be in sync with
// the constant at devtools_ui_bindings.cc.
const size_t kMaxMessageChunkSize = IPC::Channel::kMaximumMessageSize / 4;

// static
ShellDevToolsFrontend* ShellDevToolsFrontend::Show(
    WebContents* inspected_contents) {
  scoped_refptr<DevToolsAgentHost> agent(
      DevToolsAgentHost::GetOrCreateFor(inspected_contents));
  Shell* shell = Shell::CreateNewWindow(inspected_contents->GetBrowserContext(),
                                        GURL(),
                                        NULL,
                                        gfx::Size());
  ShellDevToolsFrontend* devtools_frontend = new ShellDevToolsFrontend(
      shell,
      agent.get());

  DevToolsHttpHandler* http_handler = ShellContentBrowserClient::Get()
                                          ->shell_browser_main_parts()
                                          ->devtools_http_handler();
  shell->LoadURL(http_handler->GetFrontendURL("/devtools/devtools.html"));

  return devtools_frontend;
}

void ShellDevToolsFrontend::Activate() {
  frontend_shell_->ActivateContents(web_contents());
}

void ShellDevToolsFrontend::Focus() {
  web_contents()->Focus();
}

void ShellDevToolsFrontend::InspectElementAt(int x, int y) {
  if (agent_host_)
    agent_host_->InspectElement(x, y);
}

void ShellDevToolsFrontend::Close() {
  frontend_shell_->Close();
}

void ShellDevToolsFrontend::DisconnectFromTarget() {
  if (!agent_host_)
    return;
  agent_host_->DetachClient();
  agent_host_ = NULL;
}

ShellDevToolsFrontend::ShellDevToolsFrontend(Shell* frontend_shell,
                                             DevToolsAgentHost* agent_host)
    : WebContentsObserver(frontend_shell->web_contents()),
      frontend_shell_(frontend_shell),
      agent_host_(agent_host) {
}

ShellDevToolsFrontend::~ShellDevToolsFrontend() {
}

void ShellDevToolsFrontend::RenderViewCreated(
    RenderViewHost* render_view_host) {
  if (!frontend_host_) {
    frontend_host_.reset(
        DevToolsFrontendHost::Create(web_contents()->GetMainFrame(), this));
  }
}

void ShellDevToolsFrontend::DidNavigateMainFrame(
    const LoadCommittedDetails& details,
    const FrameNavigateParams& params) {
  if (agent_host_)
    agent_host_->AttachClient(this);
}

void ShellDevToolsFrontend::WebContentsDestroyed() {
  if (agent_host_)
    agent_host_->DetachClient();
  delete this;
}

void ShellDevToolsFrontend::HandleMessageFromDevToolsFrontend(
    const std::string& message) {
  if (!agent_host_)
    return;
  std::string method;
  int id = 0;
  base::ListValue* params = NULL;
  base::DictionaryValue* dict = NULL;
  scoped_ptr<base::Value> parsed_message(base::JSONReader::Read(message));
  if (!parsed_message ||
      !parsed_message->GetAsDictionary(&dict) ||
      !dict->GetString("method", &method)) {
    return;
  }
  dict->GetList("params", &params);

  std::string browser_message;
  if (method == "sendMessageToBrowser" && params &&
      params->GetSize() == 1 && params->GetString(0, &browser_message)) {
    agent_host_->DispatchProtocolMessage(browser_message);
  } else if (method == "loadCompleted") {
    web_contents()->GetMainFrame()->ExecuteJavaScript(
        base::ASCIIToUTF16("DevToolsAPI.setUseSoftMenu(true);"));
  } else {
    return;
  }

  dict->GetInteger("id", &id);
  if (id) {
    std::string code = "DevToolsAPI.embedderMessageAck(" +
        base::IntToString(id) + ",\"\");";
    base::string16 javascript = base::UTF8ToUTF16(code);
    web_contents()->GetMainFrame()->ExecuteJavaScript(javascript);
  }
}

void ShellDevToolsFrontend::HandleMessageFromDevToolsFrontendToBackend(
    const std::string& message) {
  if (agent_host_)
    agent_host_->DispatchProtocolMessage(message);
}

void ShellDevToolsFrontend::DispatchProtocolMessage(
    DevToolsAgentHost* agent_host, const std::string& message) {

  if (message.length() < kMaxMessageChunkSize) {
    base::string16 javascript = base::UTF8ToUTF16(
        "DevToolsAPI.dispatchMessage(" + message + ");");
    web_contents()->GetMainFrame()->ExecuteJavaScript(javascript);
    return;
  }

  base::FundamentalValue total_size(static_cast<int>(message.length()));
  for (size_t pos = 0; pos < message.length(); pos += kMaxMessageChunkSize) {
    base::StringValue message_value(message.substr(pos, kMaxMessageChunkSize));
    std::string param;
    base::JSONWriter::Write(&message_value, &param);
    std::string code = "DevToolsAPI.dispatchMessageChunk(" + param + ");";
    base::string16 javascript = base::UTF8ToUTF16(code);
    web_contents()->GetMainFrame()->ExecuteJavaScript(javascript);
  }
}

void ShellDevToolsFrontend::AttachTo(WebContents* inspected_contents) {
  DisconnectFromTarget();
  agent_host_ = DevToolsAgentHost::GetOrCreateFor(inspected_contents);
}

void ShellDevToolsFrontend::AgentHostClosed(
    DevToolsAgentHost* agent_host, bool replaced) {
  frontend_shell_->Close();
}

}  // namespace content
