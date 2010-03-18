// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/javascript_execution_controller.h"

#include "base/json/string_escape.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/json_value_serializer.h"

using base::GetDoubleQuotedJson;

// JavaScriptObjectProxy methods
JavaScriptObjectProxy::JavaScriptObjectProxy(
    JavaScriptExecutionController* executor, int handle)
    : executor_(executor->AsWeakPtr()), handle_(handle) {}

JavaScriptObjectProxy::~JavaScriptObjectProxy() {
  if (is_valid())
    executor_->Remove(handle_);
}

std::string JavaScriptObjectProxy::GetReferenceJavaScript() {
  return JavaScriptExecutionController::GetReferenceJavaScript(this);
}

// JavaScriptExecutionController methods
bool JavaScriptExecutionController::ExecuteJavaScript(
    const std::string& script) {
  std::string json;
  return ExecuteJavaScript(script, &json);
}

// static
std::string JavaScriptExecutionController::GetReferenceJavaScript(
    JavaScriptObjectProxy* object) {
  return StringPrintf("domAutomation.getObject(%d)", object->handle());
}

// static
std::string JavaScriptExecutionController::Serialize(
    const std::vector<std::string>& vector) {
  std::string javascript = "[";
  for (size_t i = 0; i < vector.size(); i++) {
    javascript.append(GetDoubleQuotedJson(UTF8ToUTF16(vector[i])));
    if (i < vector.size() - 1)
      javascript.append(",");
  }
  javascript.append("]");
  return javascript;
}

void JavaScriptExecutionController::Remove(int handle) {
  ExecuteJavaScript(StringPrintf("domAutomation.removeObject(%d);", handle));
  handle_to_object_.erase(handle);
  if (handle_to_object_.empty())
    LastObjectRemoved();
}

bool JavaScriptExecutionController::ExecuteJavaScript(
    const std::string& original_script, std::string* json) {
  std::string script =
      "domAutomationController.setAutomationId(0);"
      "domAutomation.evaluateJavaScript(";
  script.append(GetDoubleQuotedJson(UTF8ToUTF16(original_script)));
  script.append(");");
  return ExecuteJavaScriptAndGetJSON(script, json);
}

bool JavaScriptExecutionController::ParseJSON(const std::string& json,
                                              scoped_ptr<Value>* result) {
  JSONStringValueSerializer parse(json);
  std::string parsing_error;
  scoped_ptr<Value> root_value(parse.Deserialize(&parsing_error));

  if (!root_value.get()) {
    if (parsing_error.length())
      LOG(ERROR) << "Cannot parse JSON response: " << parsing_error;
    else
      LOG(ERROR) << "JSON response is empty";
    return false;
  }

  // The response must be a list of 3 components:
  //   -success(boolean): whether the javascript was evaluated with no errors
  //   -error(string): the evaluation error message or the empty string if
  //       no error occurred
  //   -result(string): the result of the evaluation (in JSON), or the
  //       exact error if an error occurred (in JSON)
  bool success;
  std::string evaluation_error;
  Value* evaluation_result_value;
  if (!root_value->IsType(Value::TYPE_LIST)) {
    LOG(ERROR) << "JSON response was not in correct format";
    return false;
  }
  ListValue* list = static_cast<ListValue*>(root_value.get());
  if (!list->GetBoolean(0, &success) ||
      !list->GetString(1, &evaluation_error) ||
      !list->Remove(2, &evaluation_result_value)) {
    LOG(ERROR) << "JSON response was not in correct format";
    return false;
  }
  if (!success) {
    LOG(WARNING) << "JavaScript evaluation did not complete successfully."
                 << evaluation_error;
    return false;
  }
  result->reset(evaluation_result_value);
  return true;
}

bool JavaScriptExecutionController::ConvertResponse(Value* value,
                                                    bool* result) {
  return value->GetAsBoolean(result);
}

bool JavaScriptExecutionController::ConvertResponse(Value* value,
                                                    int* result) {
  return value->GetAsInteger(result);
}

bool JavaScriptExecutionController::ConvertResponse(Value* value,
                                                    std::string* result) {
  return value->GetAsString(result);
}
