// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_devtools_bindings.h"

#include <stddef.h>

#include <utility>

#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
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
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_response_writer.h"

#if !defined(OS_ANDROID)
#include "content/public/browser/devtools_frontend_host.h"
#endif

namespace content {

namespace {

// ResponseWriter -------------------------------------------------------------

class ResponseWriter : public net::URLFetcherResponseWriter {
 public:
  ResponseWriter(base::WeakPtr<ShellDevToolsBindings> devtools_bindings_,
                 int stream_id);
  ~ResponseWriter() override;

  // URLFetcherResponseWriter overrides:
  int Initialize(const net::CompletionCallback& callback) override;
  int Write(net::IOBuffer* buffer,
            int num_bytes,
            const net::CompletionCallback& callback) override;
  int Finish(int net_error, const net::CompletionCallback& callback) override;

 private:
  base::WeakPtr<ShellDevToolsBindings> devtools_bindings_;
  int stream_id_;

  DISALLOW_COPY_AND_ASSIGN(ResponseWriter);
};

ResponseWriter::ResponseWriter(
    base::WeakPtr<ShellDevToolsBindings> shell_devtools,
    int stream_id)
    : devtools_bindings_(shell_devtools), stream_id_(stream_id) {}

ResponseWriter::~ResponseWriter() {}

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
  base::Value* chunkValue = new base::Value(chunk);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(&ShellDevToolsBindings::CallClientFunction,
                     devtools_bindings_, "DevToolsAPI.streamWrite",
                     base::Owned(id), base::Owned(chunkValue), nullptr));
  return num_bytes;
}

int ResponseWriter::Finish(int net_error,
                           const net::CompletionCallback& callback) {
  return net::OK;
}

}  // namespace

// This constant should be in sync with
// the constant at devtools_ui_bindings.cc.
const size_t kMaxMessageChunkSize = IPC::Channel::kMaximumMessageSize / 4;

void ShellDevToolsBindings::InspectElementAt(int x, int y) {
  if (agent_host_) {
    agent_host_->InspectElement(this, x, y);
  } else {
    inspect_element_at_x_ = x;
    inspect_element_at_y_ = y;
  }
}

ShellDevToolsBindings::ShellDevToolsBindings(WebContents* devtools_contents,
                                             WebContents* inspected_contents,
                                             ShellDevToolsDelegate* delegate)
    : WebContentsObserver(devtools_contents),
      inspected_contents_(inspected_contents),
      delegate_(delegate),
      inspect_element_at_x_(-1),
      inspect_element_at_y_(-1),
      weak_factory_(this) {}

ShellDevToolsBindings::~ShellDevToolsBindings() {
  for (const auto& pair : pending_requests_)
    delete pair.first;
  if (agent_host_)
    agent_host_->DetachClient(this);
}

void ShellDevToolsBindings::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
#if !defined(OS_ANDROID)
  content::RenderFrameHost* frame = navigation_handle->GetRenderFrameHost();
  if (navigation_handle->IsInMainFrame()) {
    frontend_host_.reset(DevToolsFrontendHost::Create(
        frame,
        base::Bind(&ShellDevToolsBindings::HandleMessageFromDevToolsFrontend,
                   base::Unretained(this))));
    return;
  }
  std::string origin = navigation_handle->GetURL().GetOrigin().spec();
  auto it = extensions_api_.find(origin);
  if (it == extensions_api_.end())
    return;
  std::string script = base::StringPrintf("%s(\"%s\")", it->second.c_str(),
                                          base::GenerateGUID().c_str());
  DevToolsFrontendHost::SetupExtensionsAPI(frame, script);
#endif
}

void ShellDevToolsBindings::DocumentAvailableInMainFrame() {
  if (agent_host_)
    agent_host_->DetachClient(this);
  agent_host_ = DevToolsAgentHost::GetOrCreateFor(inspected_contents_);
  agent_host_->AttachClient(this);
  if (inspect_element_at_x_ != -1) {
    agent_host_->InspectElement(this, inspect_element_at_x_,
                                inspect_element_at_y_);
    inspect_element_at_x_ = -1;
    inspect_element_at_y_ = -1;
  }
}

void ShellDevToolsBindings::WebContentsDestroyed() {
  if (agent_host_) {
    agent_host_->DetachClient(this);
    agent_host_ = nullptr;
  }
}

void ShellDevToolsBindings::SetPreferences(const std::string& json) {
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

void ShellDevToolsBindings::HandleMessageFromDevToolsFrontend(
    const std::string& message) {
  if (!agent_host_)
    return;
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
    agent_host_->DispatchProtocolMessage(this, protocol_message);
  } else if (method == "loadCompleted") {
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("DevToolsAPI.setUseSoftMenu(true);"));
  } else if (method == "loadNetworkResource" && params->GetSize() == 3) {
    // TODO(pfeldman): handle some of the embedder messages in content.
    std::string url;
    std::string headers;
    int stream_id;
    if (!params->GetString(0, &url) || !params->GetString(1, &headers) ||
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

    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation(
            "devtools_handle_front_end_messages", R"(
            semantics {
              sender: "Developer Tools"
              description:
                "When user opens Developer Tools, the browser may fetch "
                "additional resources from the network to enrich the debugging "
                "experience (e.g. source map resources)."
              trigger: "User opens Developer Tools to debug a web page."
              data: "Any resources requested by Developer Tools."
              destination: OTHER
            }
            policy {
              cookies_allowed: YES
              cookies_store: "user"
              setting:
                "It's not possible to disable this feature from settings."
              chrome_policy {
                DeveloperToolsDisabled {
                  policy_options {mode: MANDATORY}
                  DeveloperToolsDisabled: true
                }
              }
            })");
    net::URLFetcher* fetcher =
        net::URLFetcher::Create(gurl, net::URLFetcher::GET, this,
                                traffic_annotation)
            .release();
    pending_requests_[fetcher] = request_id;
    fetcher->SetRequestContext(BrowserContext::GetDefaultStoragePartition(
                                   web_contents()->GetBrowserContext())
                                   ->GetURLRequestContext());
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
    if (!params->GetString(0, &name) || !params->GetString(1, &value)) {
      return;
    }
    preferences_.SetKey(name, base::Value(value));
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
  } else if (method == "registerExtensionsAPI") {
    std::string origin;
    std::string script;
    if (!params->GetString(0, &origin) || !params->GetString(1, &script))
      return;
    extensions_api_[origin + "/"] = script;
  } else {
    return;
  }

  if (request_id)
    SendMessageAck(request_id, nullptr);
}

void ShellDevToolsBindings::DispatchProtocolMessage(
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

void ShellDevToolsBindings::OnURLFetchComplete(const net::URLFetcher* source) {
  // TODO(pfeldman): this is a copy of chrome's devtools_ui_bindings.cc.
  // We should handle some of the commands including this one in content.
  DCHECK(source);
  PendingRequestsMap::iterator it = pending_requests_.find(source);
  DCHECK(it != pending_requests_.end());

  base::DictionaryValue response;
  auto headers = std::make_unique<base::DictionaryValue>();
  net::HttpResponseHeaders* rh = source->GetResponseHeaders();
  response.SetInteger("statusCode", rh ? rh->response_code() : 200);

  size_t iterator = 0;
  std::string name;
  std::string value;
  while (rh && rh->EnumerateHeaderLines(&iterator, &name, &value))
    headers->SetString(name, value);
  response.Set("headers", std::move(headers));

  SendMessageAck(it->second, &response);
  pending_requests_.erase(it);
  delete source;
}

void ShellDevToolsBindings::CallClientFunction(const std::string& function_name,
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

void ShellDevToolsBindings::SendMessageAck(int request_id,
                                           const base::Value* arg) {
  base::Value id_value(request_id);
  CallClientFunction("DevToolsAPI.embedderMessageAck", &id_value, arg, nullptr);
}

void ShellDevToolsBindings::AgentHostClosed(DevToolsAgentHost* agent_host) {
  agent_host_ = nullptr;
  if (delegate_)
    delegate_->Close();
}

}  // namespace content
