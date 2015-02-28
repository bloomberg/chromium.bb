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
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_response_writer.h"

namespace content {

namespace {


// ResponseWriter -------------------------------------------------------------

class ResponseWriter : public net::URLFetcherResponseWriter {
 public:
  ResponseWriter(Shell* shell, int stream_id);
  ~ResponseWriter() override;

  // URLFetcherResponseWriter overrides:
  int Initialize(const net::CompletionCallback& callback) override;
  int Write(net::IOBuffer* buffer,
            int num_bytes,
            const net::CompletionCallback& callback) override;
  int Finish(const net::CompletionCallback& callback) override;

 private:
  Shell* shell_;
  int stream_id_;

  DISALLOW_COPY_AND_ASSIGN(ResponseWriter);
};

ResponseWriter::ResponseWriter(Shell* shell,
                               int stream_id)
    : shell_(shell),
      stream_id_(stream_id) {
}

ResponseWriter::~ResponseWriter() {
}

int ResponseWriter::Initialize(const net::CompletionCallback& callback) {
  return net::OK;
}

int ResponseWriter::Write(net::IOBuffer* buffer,
                          int num_bytes,
                          const net::CompletionCallback& callback) {
  base::StringValue chunk(std::string(buffer->data(), num_bytes));
  std::string encoded;
  base::JSONWriter::Write(&chunk, &encoded);

  std::string code = base::StringPrintf(
      "DevToolsAPI.streamWrite(%d, %s)", stream_id_, encoded.c_str());
  shell_->web_contents()->GetMainFrame()->ExecuteJavaScript(
      base::UTF8ToUTF16(code));

  return num_bytes;
}

int ResponseWriter::Finish(const net::CompletionCallback& callback) {
  std::string code = base::StringPrintf(
      "DevToolsAPI.streamFinish(%d)", stream_id_);
  shell_->web_contents()->GetMainFrame()->ExecuteJavaScript(
      base::UTF8ToUTF16(code));
  return net::OK;
}

}  // namespace

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
  for (const auto& pair : pending_requests_)
    delete pair.first;
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
  base::ListValue* params = NULL;
  base::DictionaryValue* dict = NULL;
  scoped_ptr<base::Value> parsed_message(base::JSONReader::Read(message));
  if (!parsed_message ||
      !parsed_message->GetAsDictionary(&dict) ||
      !dict->GetString("method", &method)) {
    return;
  }
  int id = 0;
  dict->GetInteger("id", &id);
  dict->GetList("params", &params);

  std::string browser_message;
  if (method == "sendMessageToBrowser" && params &&
      params->GetSize() == 1 && params->GetString(0, &browser_message)) {
    agent_host_->DispatchProtocolMessage(browser_message);
  } else if (method == "loadCompleted") {
    web_contents()->GetMainFrame()->ExecuteJavaScript(
        base::ASCIIToUTF16("DevToolsAPI.setUseSoftMenu(true);"));
  } else if (method == "loadNetworkResource" && params->GetSize() == 3) {
    // TODO(pfeldman): handle some of the embedder messages in content.
    std::string url;
    std::string headers;
    int stream_id;
    if (!params->GetString(0, &url) ||
        !params->GetString(1, &headers) ||
        !params->GetInteger(2, &stream_id)) {
      return;
    }
    GURL gurl(url);
    if (!gurl.is_valid()) {
      std::string code = base::StringPrintf(
          "DevToolsAPI.embedderMessageAck(%d, { statusCode: 404 });", id);
      web_contents()->GetMainFrame()->ExecuteJavaScript(
          base::UTF8ToUTF16(code));
      return;
    }

    net::URLFetcher* fetcher =
        net::URLFetcher::Create(gurl, net::URLFetcher::GET, this);
    pending_requests_[fetcher] = id;
    fetcher->SetRequestContext(web_contents()->GetBrowserContext()->
        GetRequestContext());
    fetcher->SetExtraRequestHeaders(headers);
    fetcher->SaveResponseWithWriter(scoped_ptr<net::URLFetcherResponseWriter>(
        new ResponseWriter(frontend_shell(), stream_id)));
    fetcher->Start();
    return;
  } else {
    return;
  }

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

void ShellDevToolsFrontend::OnURLFetchComplete(const net::URLFetcher* source) {
  // TODO(pfeldman): this is a copy of chrome's devtools_ui_bindings.cc.
  // We should handle some of the commands including this one in content.
  DCHECK(source);
  PendingRequestsMap::iterator it = pending_requests_.find(source);
  DCHECK(it != pending_requests_.end());

  base::DictionaryValue response;
  base::DictionaryValue* headers = new base::DictionaryValue();
  net::HttpResponseHeaders* rh = source->GetResponseHeaders();
  response.SetInteger("statusCode", rh ? rh->response_code() : 200);
  response.Set("headers", headers);

  void* iterator = NULL;
  std::string name;
  std::string value;
  while (rh && rh->EnumerateHeaderLines(&iterator, &name, &value))
    headers->SetString(name, value);

  std::string json;
  base::JSONWriter::Write(&response, &json);

  std::string message = base::StringPrintf(
      "DevToolsAPI.embedderMessageAck(%d, %s)",
      it->second,
      json.c_str());
  web_contents()->GetMainFrame()->
      ExecuteJavaScript(base::UTF8ToUTF16(message));

  pending_requests_.erase(it);
  delete source;
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
