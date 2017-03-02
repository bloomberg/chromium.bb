// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_devtools_frontend.h"

#include <stddef.h>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_devtools_manager_delegate.h"
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
  ResponseWriter(base::WeakPtr<ShellDevToolsFrontend> shell_devtools_,
                 int stream_id);
  ~ResponseWriter() override;

  // URLFetcherResponseWriter overrides:
  int Initialize(const net::CompletionCallback& callback) override;
  int Write(net::IOBuffer* buffer,
            int num_bytes,
            const net::CompletionCallback& callback) override;
  int Finish(int net_error, const net::CompletionCallback& callback) override;

 private:
  base::WeakPtr<ShellDevToolsFrontend> shell_devtools_;
  int stream_id_;

  DISALLOW_COPY_AND_ASSIGN(ResponseWriter);
};

ResponseWriter::ResponseWriter(
    base::WeakPtr<ShellDevToolsFrontend> shell_devtools,
    int stream_id)
    : shell_devtools_(shell_devtools),
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
  std::string chunk = std::string(buffer->data(), num_bytes);
  if (!base::IsStringUTF8(chunk))
    return num_bytes;

  base::Value* id = new base::Value(stream_id_);
  base::StringValue* chunkValue = new base::StringValue(chunk);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&ShellDevToolsFrontend::CallClientFunction,
                 shell_devtools_, "DevToolsAPI.streamWrite",
                 base::Owned(id), base::Owned(chunkValue), nullptr));
  return num_bytes;
}

int ResponseWriter::Finish(int net_error,
                           const net::CompletionCallback& callback) {
  return net::OK;
}

static GURL GetFrontendURL() {
  int port = ShellDevToolsManagerDelegate::GetHttpHandlerPort();
  return GURL(
      base::StringPrintf("http://127.0.0.1:%d/devtools/inspector.html", port));
}

}  // namespace

// This constant should be in sync with
// the constant at devtools_ui_bindings.cc.
const size_t kMaxMessageChunkSize = IPC::Channel::kMaximumMessageSize / 4;

// static
ShellDevToolsFrontend* ShellDevToolsFrontend::Show(
    WebContents* inspected_contents) {
  Shell* shell = Shell::CreateNewWindow(inspected_contents->GetBrowserContext(),
                                        GURL(),
                                        NULL,
                                        gfx::Size());
  ShellDevToolsFrontend* devtools_frontend = new ShellDevToolsFrontend(
      shell,
      inspected_contents);
  shell->LoadURL(GetFrontendURL());
  return devtools_frontend;
}

void ShellDevToolsFrontend::Activate() {
  frontend_shell_->ActivateContents(web_contents());
}

void ShellDevToolsFrontend::Focus() {
  web_contents()->Focus();
}

void ShellDevToolsFrontend::InspectElementAt(int x, int y) {
  if (agent_host_) {
    agent_host_->InspectElement(this, x, y);
  } else {
    inspect_element_at_x_ = x;
    inspect_element_at_y_ = y;
  }
}

void ShellDevToolsFrontend::Close() {
  frontend_shell_->Close();
}

void ShellDevToolsFrontend::DisconnectFromTarget() {
  if (!agent_host_)
    return;
  agent_host_->DetachClient(this);
  agent_host_ = NULL;
}

ShellDevToolsFrontend::ShellDevToolsFrontend(Shell* frontend_shell,
                                             WebContents* inspected_contents)
    : WebContentsObserver(frontend_shell->web_contents()),
      frontend_shell_(frontend_shell),
      inspected_contents_(inspected_contents),
      inspect_element_at_x_(-1),
      inspect_element_at_y_(-1),
      weak_factory_(this) {
}

ShellDevToolsFrontend::~ShellDevToolsFrontend() {
  for (const auto& pair : pending_requests_)
    delete pair.first;
}

void ShellDevToolsFrontend::RenderViewCreated(
    RenderViewHost* render_view_host) {
  if (!frontend_host_) {
    frontend_host_.reset(DevToolsFrontendHost::Create(
        web_contents()->GetMainFrame(),
        base::Bind(&ShellDevToolsFrontend::HandleMessageFromDevToolsFrontend,
                   base::Unretained(this))));
  }
}

void ShellDevToolsFrontend::DocumentAvailableInMainFrame() {
  agent_host_ = DevToolsAgentHost::GetOrCreateFor(inspected_contents_);
  agent_host_->AttachClient(this);
  if (inspect_element_at_x_ != -1) {
    agent_host_->InspectElement(
        this, inspect_element_at_x_, inspect_element_at_y_);
    inspect_element_at_x_ = -1;
    inspect_element_at_y_ = -1;
  }
}

void ShellDevToolsFrontend::WebContentsDestroyed() {
  if (agent_host_)
    agent_host_->DetachClient(this);
  delete this;
}

void ShellDevToolsFrontend::SetPreferences(const std::string& json) {
  preferences_.Clear();
  if (json.empty())
    return;
  base::DictionaryValue* dict = nullptr;
  std::unique_ptr<base::Value> parsed = base::JSONReader::Read(json);
  if (!parsed || !parsed->GetAsDictionary(&dict))
    return;
  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    if (!it.value().IsType(base::Value::Type::STRING))
      continue;
    preferences_.SetWithoutPathExpansion(it.key(), it.value().CreateDeepCopy());
  }
}

void ShellDevToolsFrontend::HandleMessageFromDevToolsFrontend(
    const std::string& message) {
  if (!agent_host_)
    return;
  std::string method;
  base::ListValue* params = NULL;
  base::DictionaryValue* dict = NULL;
  std::unique_ptr<base::Value> parsed_message = base::JSONReader::Read(message);
  if (!parsed_message ||
      !parsed_message->GetAsDictionary(&dict) ||
      !dict->GetString("method", &method)) {
    return;
  }
  int request_id = 0;
  dict->GetInteger("id", &request_id);
  dict->GetList("params", &params);

  if (method == "dispatchProtocolMessage" && params && params->GetSize() == 1) {
    if (!agent_host_ || !agent_host_->IsAttached())
      return;
    std::string protocol_message;
    if (!params->GetString(0, &protocol_message))
      return;
    agent_host_->DispatchProtocolMessage(this, protocol_message);
  } else if (method == "loadCompleted") {
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
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
      base::DictionaryValue response;
      response.SetInteger("statusCode", 404);
      SendMessageAck(request_id, &response);
      return;
    }

    net::URLFetcher* fetcher =
        net::URLFetcher::Create(gurl, net::URLFetcher::GET, this).release();
    pending_requests_[fetcher] = request_id;
    fetcher->SetRequestContext(
        BrowserContext::GetDefaultStoragePartition(
            web_contents()->GetBrowserContext())->
                GetURLRequestContext());
    fetcher->SetExtraRequestHeaders(headers);
    fetcher->SaveResponseWithWriter(
        std::unique_ptr<net::URLFetcherResponseWriter>(
            new ResponseWriter(weak_factory_.GetWeakPtr(), stream_id)));
    fetcher->Start();
    return;
  } else if (method == "getPreferences") {
    SendMessageAck(request_id, &preferences_);
    return;
  } else if (method == "setPreference") {
    std::string name;
    std::string value;
    if (!params->GetString(0, &name) ||
        !params->GetString(1, &value)) {
      return;
    }
    preferences_.SetStringWithoutPathExpansion(name, value);
  } else if (method == "removePreference") {
    std::string name;
    if (!params->GetString(0, &name))
      return;
    preferences_.RemoveWithoutPathExpansion(name, nullptr);
  } else if (method == "requestFileSystems") {
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("DevToolsAPI.fileSystemsLoaded([]);"));
  } else if (method == "reattach") {
    agent_host_->DetachClient(this);
    agent_host_->AttachClient(this);
  } else {
    return;
  }

  if (request_id)
    SendMessageAck(request_id, nullptr);
}

void ShellDevToolsFrontend::DispatchProtocolMessage(
    DevToolsAgentHost* agent_host, const std::string& message) {

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

  size_t iterator = 0;
  std::string name;
  std::string value;
  while (rh && rh->EnumerateHeaderLines(&iterator, &name, &value))
    headers->SetString(name, value);

  SendMessageAck(it->second, &response);
  pending_requests_.erase(it);
  delete source;
}

void ShellDevToolsFrontend::CallClientFunction(
    const std::string& function_name,
    const base::Value* arg1,
    const base::Value* arg2,
    const base::Value* arg3) {
  std::string javascript = function_name + "(";
  if (arg1) {
    std::string json;
    base::JSONWriter::Write(*arg1, &json);
    javascript.append(json);
    if (arg2) {
      base::JSONWriter::Write(*arg2, &json);
      javascript.append(", ").append(json);
      if (arg3) {
        base::JSONWriter::Write(*arg3, &json);
        javascript.append(", ").append(json);
      }
    }
  }
  javascript.append(");");
  web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(javascript));
}

void ShellDevToolsFrontend::SendMessageAck(int request_id,
                                           const base::Value* arg) {
  base::Value id_value(request_id);
  CallClientFunction("DevToolsAPI.embedderMessageAck",
                     &id_value, arg, nullptr);
}

void ShellDevToolsFrontend::AgentHostClosed(
    DevToolsAgentHost* agent_host, bool replaced) {
  agent_host_ = nullptr;
  frontend_shell_->Close();
}

}  // namespace content
