// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/web_view_impl.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/test/chromedriver/devtools_client_impl.h"
#include "chrome/test/chromedriver/dom_tracker.h"
#include "chrome/test/chromedriver/frame_tracker.h"
#include "chrome/test/chromedriver/javascript_dialog_manager.h"
#include "chrome/test/chromedriver/js.h"
#include "chrome/test/chromedriver/navigation_tracker.h"
#include "chrome/test/chromedriver/status.h"
#include "chrome/test/chromedriver/ui_events.h"
#include "chrome/test/chromedriver/web_view_delegate.h"

namespace {

Status GetContextIdForFrame(FrameTracker* tracker,
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

const char* GetAsString(MouseEventType type) {
  switch (type) {
    case kPressedMouseEventType:
      return "mousePressed";
    case kReleasedMouseEventType:
      return "mouseReleased";
    case kMovedMouseEventType:
      return "mouseMoved";
    default:
      return "";
  }
}

const char* GetAsString(MouseButton button) {
  switch (button) {
    case kLeftMouseButton:
      return "left";
    case kMiddleMouseButton:
      return "middle";
    case kRightMouseButton:
      return "right";
    case kNoneMouseButton:
      return "none";
    default:
      return "";
  }
}

bool IsNotPendingNavigation(NavigationTracker* tracker,
                            const std::string& frame_id) {
  return !tracker->IsPendingNavigation(frame_id);
}

const char* GetAsString(KeyEventType type) {
  switch (type) {
    case kKeyDownEventType:
      return "keyDown";
    case kKeyUpEventType:
      return "keyUp";
    case kRawKeyDownEventType:
      return "rawKeyDown";
    case kCharEventType:
      return "char";
    default:
      return "";
  }
}

}  // namespace

WebViewImpl::WebViewImpl(const std::string& id,
                         DevToolsClient* client,
                         WebViewDelegate* delegate,
                         const CloserFunc& closer_func)
    : id_(id),
      dom_tracker_(new DomTracker(client)),
      frame_tracker_(new FrameTracker(client)),
      navigation_tracker_(new NavigationTracker(client)),
      dialog_manager_(new JavaScriptDialogManager(client)),
      client_(client),
      delegate_(delegate),
      closer_func_(closer_func) {}

WebViewImpl::~WebViewImpl() {}

std::string WebViewImpl::GetId() {
  return id_;
}

Status WebViewImpl::Close() {
  Status status = closer_func_.Run();
  if (status.IsOk())
    delegate_->OnWebViewClose(this);
  return status;
}

Status WebViewImpl::Load(const std::string& url) {
  base::DictionaryValue params;
  params.SetString("url", url);
  return client_->SendCommand("Page.navigate", params);
}

Status WebViewImpl::Reload() {
  base::DictionaryValue params;
  params.SetBoolean("ignoreCache", false);
  return client_->SendCommand("Page.reload", params);
}

Status WebViewImpl::EvaluateScript(const std::string& frame,
                                   const std::string& expression,
                                   scoped_ptr<base::Value>* result) {
  int context_id;
  Status status = GetContextIdForFrame(frame_tracker_.get(), frame,
                                       &context_id);
  if (status.IsError())
    return status;
  return internal::EvaluateScriptAndGetValue(
      client_.get(), context_id, expression, result);
}

Status WebViewImpl::CallFunction(const std::string& frame,
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
  scoped_ptr<base::Value> temp_result;
  Status status = EvaluateScript(frame, expression, &temp_result);
  if (status.IsError())
    return status;

  return internal::ParseCallFunctionResult(*temp_result, result);
}

Status WebViewImpl::GetFrameByFunction(const std::string& frame,
                                       const std::string& function,
                                       const base::ListValue& args,
                                       std::string* out_frame) {
  int context_id;
  Status status = GetContextIdForFrame(frame_tracker_.get(), frame,
                                       &context_id);
  if (status.IsError())
    return status;
  int node_id;
  status = internal::GetNodeIdFromFunction(
      client_.get(), context_id, function, args, &node_id);
  if (status.IsError())
    return status;
  return dom_tracker_->GetFrameIdForNode(node_id, out_frame);
}

Status WebViewImpl::DispatchMouseEvents(const std::list<MouseEvent>& events) {
  for (std::list<MouseEvent>::const_iterator it = events.begin();
       it != events.end(); ++it) {
    base::DictionaryValue params;
    params.SetString("type", GetAsString(it->type));
    params.SetInteger("x", it->x);
    params.SetInteger("y", it->y);
    params.SetString("button", GetAsString(it->button));
    params.SetInteger("clickCount", it->click_count);
    Status status = client_->SendCommand("Input.dispatchMouseEvent", params);
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

Status WebViewImpl::DispatchKeyEvents(const std::list<KeyEvent>& events) {
  for (std::list<KeyEvent>::const_iterator it = events.begin();
       it != events.end(); ++it) {
    base::DictionaryValue params;
    params.SetString("type", GetAsString(it->type));
    if (it->modifiers & kNumLockKeyModifierMask) {
      params.SetBoolean("isKeypad", true);
      params.SetInteger("modifiers",
                        it->modifiers & ~kNumLockKeyModifierMask);
    } else {
      params.SetInteger("modifiers", it->modifiers);
    }
    params.SetString("text", it->modified_text);
    params.SetString("unmodifiedText", it->unmodified_text);
    params.SetInteger("nativeVirtualKeyCode", it->key_code);
    params.SetInteger("windowsVirtualKeyCode", it->key_code);
    Status status = client_->SendCommand("Input.dispatchKeyEvent", params);
    if (status.IsError())
      return status;
  }
  return Status(kOk);
}

Status WebViewImpl::WaitForPendingNavigations(const std::string& frame_id) {
  std::string full_frame_id(frame_id);
  if (full_frame_id.empty()) {
    Status status = GetMainFrame(&full_frame_id);
    if (status.IsError())
      return status;
  }
  return client_->HandleEventsUntil(
      base::Bind(IsNotPendingNavigation, navigation_tracker_.get(),
                 full_frame_id));
}

Status WebViewImpl::GetMainFrame(std::string* out_frame) {
  base::DictionaryValue params;
  scoped_ptr<base::DictionaryValue> result;
  Status status = client_->SendCommandAndGetResult(
      "Page.getResourceTree", params, &result);
  if (status.IsError())
    return status;
  if (!result->GetString("frameTree.frame.id", out_frame))
    return Status(kUnknownError, "missing 'frameTree.frame.id' in response");
  return Status(kOk);
}

JavaScriptDialogManager* WebViewImpl::GetJavaScriptDialogManager() {
  return dialog_manager_.get();
}

Status WebViewImpl::CaptureScreenshot(std::string* screenshot) {
  base::DictionaryValue params;
  scoped_ptr<base::DictionaryValue> result;
  Status status = client_->SendCommandAndGetResult(
      "Page.captureScreenshot", params, &result);
  if (status.IsError())
    return status;
  if (!result->GetString("data", screenshot))
    return Status(kUnknownError, "expected string 'data' in response");
  return Status(kOk);
}

namespace internal {

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
    base::Value* value;
    if (!temp_result->Get("value", &value))
      return Status(kUnknownError, "Runtime.evaluate missing 'value'");
    result->reset(value->DeepCopy());
  }
  return Status(kOk);
}

Status ParseCallFunctionResult(const base::Value& temp_result,
                               scoped_ptr<base::Value>* result) {
  const base::DictionaryValue* dict;
  if (!temp_result.GetAsDictionary(&dict))
    return Status(kUnknownError, "call function result must be a dictionary");
  int status_code;
  if (!dict->GetInteger("status", &status_code)) {
    return Status(kUnknownError,
                  "call function result missing int 'status'");
  }
  if (status_code != kOk)
    return Status(static_cast<StatusCode>(status_code));
  const base::Value* unscoped_value;
  if (!dict->Get("value", &unscoped_value)) {
    return Status(kUnknownError,
                  "call function result missing 'value'");
  }
  result->reset(unscoped_value->DeepCopy());
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
