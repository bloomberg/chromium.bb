// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/devtools_protocol_test_bindings.h"

#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

#if !defined(OS_ANDROID)
#include "content/public/browser/devtools_frontend_host.h"
#endif

namespace content {

namespace {
// This constant should be in sync with
// the constant at devtools_ui_bindings.cc.
const size_t kMaxMessageChunkSize = IPC::Channel::kMaximumMessageSize / 4;
}  // namespace

DevToolsProtocolTestBindings::DevToolsProtocolTestBindings(
    WebContents* devtools)
    : WebContentsObserver(devtools),
      agent_host_(DevToolsAgentHost::CreateForDiscovery()) {
  agent_host_->AttachClient(this);
}

DevToolsProtocolTestBindings::~DevToolsProtocolTestBindings() {
  if (agent_host_) {
    agent_host_->DetachClient(this);
    agent_host_ = nullptr;
  }
}

// static
GURL DevToolsProtocolTestBindings::MapTestURLIfNeeded(const GURL& test_url,
                                                      bool* is_protocol_test) {
  *is_protocol_test = false;
  std::string spec = test_url.spec();
  std::string dir = "/inspector-protocol/";
  size_t pos = spec.find(dir);
  if (pos == std::string::npos)
    return test_url;
  if (spec.rfind(".js") != spec.length() - 3)
    return test_url;
  spec = spec.substr(0, pos + dir.length()) +
         "resources/inspector-protocol-test.html?" + spec;
  *is_protocol_test = true;
  return GURL(spec);
}

void DevToolsProtocolTestBindings::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
#if !defined(OS_ANDROID)
  content::RenderFrameHost* frame = navigation_handle->GetRenderFrameHost();
  if (frame->GetParent())
    return;
  frontend_host_.reset(DevToolsFrontendHost::Create(
      frame, base::Bind(&DevToolsProtocolTestBindings::HandleMessageFromTest,
                        base::Unretained(this))));
#endif
}

void DevToolsProtocolTestBindings::WebContentsDestroyed() {
  if (agent_host_) {
    agent_host_->DetachClient(this);
    agent_host_ = nullptr;
  }
}

void DevToolsProtocolTestBindings::HandleMessageFromTest(
    const std::string& message) {
  std::string method;
  base::ListValue* params = nullptr;
  base::DictionaryValue* dict = nullptr;
  std::unique_ptr<base::Value> parsed_message = base::JSONReader::Read(message);
  if (!parsed_message || !parsed_message->GetAsDictionary(&dict) ||
      !dict->GetString("method", &method)) {
    return;
  }

  int request_id = 0;
  dict->GetInteger("id", &request_id);
  dict->GetList("params", &params);

  if (method == "dispatchProtocolMessage" && params && params->GetSize() == 1) {
    std::string protocol_message;
    if (!params->GetString(0, &protocol_message))
      return;
    if (agent_host_)
      agent_host_->DispatchProtocolMessage(this, protocol_message);
    return;
  }
}

void DevToolsProtocolTestBindings::DispatchProtocolMessage(
    DevToolsAgentHost* agent_host,
    const std::string& message) {
  if (message.length() < kMaxMessageChunkSize) {
    std::string param;
    base::EscapeJSONString(message, true, &param);
    std::string code = "DevToolsAPI.dispatchMessage(" + param + ");";
    base::string16 javascript = base::UTF8ToUTF16(code);
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(javascript);
    return;
  }

  size_t total_size = message.length();
  for (size_t pos = 0; pos < message.length(); pos += kMaxMessageChunkSize) {
    std::string param;
    base::EscapeJSONString(message.substr(pos, kMaxMessageChunkSize), true,
                           &param);
    std::string code = "DevToolsAPI.dispatchMessageChunk(" + param + "," +
                       std::to_string(pos ? 0 : total_size) + ");";
    base::string16 javascript = base::UTF8ToUTF16(code);
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(javascript);
  }
}

void DevToolsProtocolTestBindings::AgentHostClosed(
    DevToolsAgentHost* agent_host) {
  agent_host_ = nullptr;
}

}  // namespace content
