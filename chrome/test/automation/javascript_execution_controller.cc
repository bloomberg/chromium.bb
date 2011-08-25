// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/automation/javascript_execution_controller.h"

#include "chrome/test/automation/javascript_message_utils.h"
#include "content/common/json_value_serializer.h"

using javascript_utils::JavaScriptPrintf;

// Initialize this timeout to an invalid value. Each test framework or test
// must set an appropriate timeout using set_timeout, or the
// JavaScriptExecutionController will complain.
int JavaScriptExecutionController::timeout_ms_ = -1;

JavaScriptExecutionController::JavaScriptExecutionController() {}

JavaScriptExecutionController::~JavaScriptExecutionController() {}

bool JavaScriptExecutionController::ExecuteJavaScript(
    const std::string& script) {
  scoped_ptr<Value> return_value;
  return ExecuteAndParseHelper(WrapJavaScript(script), &return_value);
}

bool JavaScriptExecutionController::ExecuteAsyncJavaScript(
    const std::string& script) {
  scoped_ptr<Value> return_value;
  return ExecuteAndParseHelper(WrapAsyncJavaScript(script), &return_value);
}

void JavaScriptExecutionController::Remove(int handle) {
  handle_to_object_.erase(handle);
  if (handle_to_object_.empty())
    LastObjectRemoved();
}

std::string JavaScriptExecutionController::WrapJavaScript(
    const std::string& original_script) {
  const char* script =
      "domAutomationController.setAutomationId(0);"
      "domAutomation.evaluateJavaScript(%s);";
  return JavaScriptPrintf(script, original_script);
}

std::string JavaScriptExecutionController::WrapAsyncJavaScript(
    const std::string& original_script) {
  if (timeout_ms_ == -1) {
    NOTREACHED() << "Timeout for asynchronous JavaScript methods has not been "
                 << "set. Call JavaScriptExecutionController::"
                 << "set_timeout(timeout_in_ms).";
  }
  const char* script =
      "domAutomationController.setAutomationId(0);"
      "domAutomation.evaluateAsyncJavaScript(%s, %s);";
  return JavaScriptPrintf(script, original_script, timeout_ms_);
}

bool JavaScriptExecutionController::ExecuteAndParseHelper(
    const std::string& script, scoped_ptr<Value>* result) {
  std::string json;
  if (!ExecuteJavaScriptAndGetJSON(script, &json)) {
    LOG(ERROR) << "JavaScript either did not execute or did not respond.";
    return false;
  }

  // Deserialize the json to a Value.
  JSONStringValueSerializer parse(json);
  std::string parsing_error;
  scoped_ptr<Value> root_value(parse.Deserialize(NULL, &parsing_error));

  // Parse the response.
  // The response must be a list of 3 components:
  //   - success (boolean): whether the javascript was evaluated with no errors
  //   - error (string): the evaluation error message or the empty string if
  //       no error occurred
  //   - result (string): the result of the evaluation (in JSON), or the
  //       exact error if an error occurred (in JSON)
  if (!root_value.get()) {
    if (parsing_error.length()) {
      LOG(ERROR) << "Cannot parse JSON response: " << parsing_error;
    } else {
      LOG(ERROR) << "JSON response is empty";
    }
    return false;
  }

  bool success;
  std::string evaluation_error;
  Value* evaluation_result_value;
  ListValue* list = root_value->AsList();
  if (!list) {
    LOG(ERROR) << "JSON response was not in correct format";
    return false;
  }
  if (!list->GetBoolean(0, &success) ||
      !list->GetString(1, &evaluation_error) ||
      !list->Remove(2, &evaluation_result_value)) {
    LOG(ERROR) << "JSON response was not in correct format";
    return false;
  }
  if (!success) {
    LOG(WARNING) << "JavaScript evaluation did not complete successfully: "
                 << evaluation_error;
    return false;
  }
  result->reset(evaluation_result_value);
  return true;
}
