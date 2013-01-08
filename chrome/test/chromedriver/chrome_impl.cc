// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome_impl.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/devtools_client_impl.h"
#include "chrome/test/chromedriver/dom_tracker.h"
#include "chrome/test/chromedriver/js.h"
#include "chrome/test/chromedriver/net/net_util.h"
#include "chrome/test/chromedriver/net/sync_websocket_impl.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "chrome/test/chromedriver/status.h"
#include "googleurl/src/gurl.h"

namespace {

Status FetchPagesInfo(URLRequestContextGetter* context_getter,
                      int port,
                      std::list<std::string>* debugger_urls) {
  std::string url = base::StringPrintf(
      "http://127.0.0.1:%d/json", port);
  std::string data;
  if (!FetchUrl(GURL(url), context_getter, &data))
    return Status(kChromeNotReachable);
  return internal::ParsePagesInfo(data, debugger_urls);
}

Status GetContextIdForFrame(DomTracker* tracker,
                            const std::string& frame,
                            int* context_id) {
  if (frame.empty()) {
    *context_id = 0;
    return Status(kOk);
  }
  Status status = tracker->GetContextIdForFrame(frame, context_id);
  if (status.IsError())
    return status;
  return Status(kOk);
}

}  // namespace

ChromeImpl::ChromeImpl(base::ProcessHandle process,
                       URLRequestContextGetter* context_getter,
                       base::ScopedTempDir* user_data_dir,
                       int port,
                       const SyncWebSocketFactory& socket_factory)
    : process_(process),
      context_getter_(context_getter),
      port_(port),
      socket_factory_(socket_factory),
      dom_tracker_(new DomTracker()) {
  if (user_data_dir->IsValid()) {
    CHECK(user_data_dir_.Set(user_data_dir->Take()));
  }
}

ChromeImpl::~ChromeImpl() {
  base::CloseProcessHandle(process_);
}

Status ChromeImpl::Init() {
  base::Time deadline = base::Time::Now() + base::TimeDelta::FromSeconds(20);
  std::list<std::string> debugger_urls;
  while (base::Time::Now() < deadline) {
    FetchPagesInfo(context_getter_, port_, &debugger_urls);
    if (debugger_urls.empty())
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
    else
      break;
  }
  if (debugger_urls.empty())
    return Status(kUnknownError, "unable to discover open pages");
  client_.reset(new DevToolsClientImpl(
      socket_factory_, debugger_urls.front(), dom_tracker_.get()));

  // Perform necessary configuration of the DevTools client.
  // Fetch the root document node so that Inspector will push DOM node
  // information to the client.
  base::DictionaryValue params;
  Status status = client_->SendCommand("DOM.getDocument", params);
  if (status.IsError())
    return status;
  // Enable runtime events to allow tracking execution context creation.
  return client_->SendCommand("Runtime.enable", params);
}

Status ChromeImpl::Load(const std::string& url) {
  base::DictionaryValue params;
  params.SetString("url", url);
  return client_->SendCommand("Page.navigate", params);
}

Status ChromeImpl::EvaluateScript(const std::string& frame,
                                  const std::string& expression,
                                  scoped_ptr<base::Value>* result) {
  int context_id;
  Status status = GetContextIdForFrame(dom_tracker_.get(), frame, &context_id);
  if (status.IsError())
    return status;
  return internal::EvaluateScriptAndGetValue(
      client_.get(), context_id, expression, result);
}

Status ChromeImpl::CallFunction(const std::string& frame,
                                const std::string& function,
                                const base::ListValue& args,
                                scoped_ptr<base::Value>* result) {
  std::string json;
  base::JSONWriter::Write(&args, &json);
  std::string expression = base::StringPrintf(
      "(%s).apply(null, [%s, %s])",
      kCallFunctionScript,
      function.c_str(),
      json.c_str());
  return EvaluateScript(frame, expression, result);
}

Status ChromeImpl::GetFrameByFunction(const std::string& frame,
                                      const std::string& function,
                                      const base::ListValue& args,
                                      std::string* out_frame) {
  int context_id;
  Status status = GetContextIdForFrame(dom_tracker_.get(), frame, &context_id);
  if (status.IsError())
    return status;
  int node_id;
  status = internal::GetNodeIdFromFunction(
      client_.get(), context_id, function, args, &node_id);
  if (status.IsError())
    return status;
  return dom_tracker_->GetFrameIdForNode(node_id, out_frame);
}

Status ChromeImpl::Quit() {
  if (!base::KillProcess(process_, 0, true))
    return Status(kUnknownError, "cannot kill Chrome");
  return Status(kOk);
}

namespace internal {

Status ParsePagesInfo(const std::string& data,
                      std::list<std::string>* debugger_urls) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(data));
  if (!value.get())
    return Status(kUnknownError, "DevTools returned invalid JSON");
  base::ListValue* list;
  if (!value->GetAsList(&list))
    return Status(kUnknownError, "DevTools did not return list");

  std::list<std::string> internal_urls;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    base::DictionaryValue* info;
    if (!list->GetDictionary(i, &info))
      return Status(kUnknownError, "DevTools contains non-dictionary item");
    std::string debugger_url;
    if (!info->GetString("webSocketDebuggerUrl", &debugger_url))
      return Status(kUnknownError, "DevTools did not include debugger URL");
    internal_urls.push_back(debugger_url);
  }
  debugger_urls->swap(internal_urls);
  return Status(kOk);
}

Status EvaluateScript(DevToolsClient* client,
                      int context_id,
                      const std::string& expression,
                      EvaluateScriptReturnType return_type,
                      scoped_ptr<base::DictionaryValue>* result) {
  base::DictionaryValue params;
  params.SetString("expression", expression);
  if (context_id)
    params.SetInteger("contextId", context_id);
  params.SetBoolean("returnByValue", return_type == ReturnByValue);
  scoped_ptr<base::DictionaryValue> cmd_result;
  Status status = client->SendCommandAndGetResult(
      "Runtime.evaluate", params, &cmd_result);
  if (status.IsError())
    return status;

  bool was_thrown;
  if (!cmd_result->GetBoolean("wasThrown", &was_thrown))
    return Status(kUnknownError, "Runtime.evaluate missing 'wasThrown'");
  if (was_thrown) {
    std::string description = "unknown";
    cmd_result->GetString("result.description", &description);
    return Status(kUnknownError,
                  "Runtime.evaluate threw exception: " + description);
  }

  base::DictionaryValue* unscoped_result;
  if (!cmd_result->GetDictionary("result", &unscoped_result))
    return Status(kUnknownError, "evaluate missing dictionary 'result'");
  result->reset(unscoped_result->DeepCopy());
  return Status(kOk);
}

Status EvaluateScriptAndGetObject(DevToolsClient* client,
                                  int context_id,
                                  const std::string& expression,
                                  std::string* object_id) {
  scoped_ptr<base::DictionaryValue> result;
  Status status = EvaluateScript(client, context_id, expression, ReturnByObject,
                                 &result);
  if (status.IsError())
    return status;
  if (!result->GetString("objectId", object_id))
    return Status(kUnknownError, "evaluate missing string 'objectId'");
  return Status(kOk);
}

Status EvaluateScriptAndGetValue(DevToolsClient* client,
                                 int context_id,
                                 const std::string& expression,
                                 scoped_ptr<base::Value>* result) {
  scoped_ptr<base::DictionaryValue> temp_result;
  Status status = EvaluateScript(client, context_id, expression, ReturnByValue,
                                 &temp_result);
  if (status.IsError())
    return status;

  std::string type;
  if (!temp_result->GetString("type", &type))
    return Status(kUnknownError, "Runtime.evaluate missing string 'type'");

  if (type == "undefined") {
    result->reset(base::Value::CreateNullValue());
  } else {
    int status_code;
    if (!temp_result->GetInteger("value.status", &status_code)) {
      return Status(kUnknownError,
                    "Runtime.evaluate missing int 'value.status'");
    }
    if (status_code != kOk)
      return Status(static_cast<StatusCode>(status_code));
    base::Value* unscoped_value;
    if (!temp_result->Get("value.value", &unscoped_value)) {
      return Status(kUnknownError,
                    "Runtime.evaluate missing 'value.value'");
    }
    result->reset(unscoped_value->DeepCopy());
  }
  return Status(kOk);
}

Status GetNodeIdFromFunction(DevToolsClient* client,
                             int context_id,
                             const std::string& function,
                             const base::ListValue& args,
                             int* node_id) {
  std::string json;
  base::JSONWriter::Write(&args, &json);
  std::string expression = base::StringPrintf(
      "(%s).apply(null, [%s, %s, true])",
      kCallFunctionScript,
      function.c_str(),
      json.c_str());

  std::string element_id;
  Status status = internal::EvaluateScriptAndGetObject(
      client, context_id, expression, &element_id);
  if (status.IsError())
    return status;

  scoped_ptr<base::DictionaryValue> cmd_result;
  {
    base::DictionaryValue params;
    params.SetString("objectId", element_id);
    status = client->SendCommandAndGetResult(
        "DOM.requestNode", params, &cmd_result);
  }
  {
    // Release the remote object before doing anything else.
    base::DictionaryValue params;
    params.SetString("objectId", element_id);
    Status release_status =
        client->SendCommand("Runtime.releaseObject", params);
    if (release_status.IsError()) {
      LOG(ERROR) << "Failed to release remote object: "
                 << release_status.message();
    }
  }
  if (status.IsError())
    return status;

  if (!cmd_result->GetInteger("nodeId", node_id))
    return Status(kUnknownError, "DOM.requestNode missing int 'nodeId'");
  return Status(kOk);
}

}  // namespace internal
