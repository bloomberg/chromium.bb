// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/webelement_commands.h"

#include "base/scoped_ptr.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/error_codes.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/utility_functions.h"
#include "third_party/webdriver/atoms.h"

namespace webdriver {

bool WebElementCommand::Init(Response* const response) {
  if (!WebDriverCommand::Init(response))
    return false;

  // There should be at least 5 segments to match
  // "/session/$session/element/$id"
  if (path_segments_.size() < 5) {
    SET_WEBDRIVER_ERROR(response, "Path segments is less than 5",
                        kBadRequest);
    return false;
  }

  // We cannot verify the ID is valid until we execute the command and
  // inject the ID into the in-page cache.
  element_id = path_segments_.at(4);
  return true;
}

bool WebElementCommand::GetElementLocation(bool in_view, int* x, int* y) {
  scoped_ptr<ListValue> args(new ListValue());
  Value* result = NULL;

  std::string jscript = build_atom(GET_LOCATION, sizeof GET_LOCATION);
  if (in_view) {
    jscript.append("arguments[0].scrollIntoView();");
  }
  jscript.append("return getLocation(arguments[0]);");

  args->Append(GetElementIdAsDictionaryValue(element_id));

  ErrorCode error = session_->ExecuteScript(jscript, args.get(), &result);
  if (error != kSuccess) {
    LOG(INFO) << "Javascript failed to execute" << std::endl;
    return false;
  }

  if (result == NULL || result->GetType() != Value::TYPE_DICTIONARY) {
    LOG(ERROR) << "Expected JavaScript atom to return a dictionary";
    return false;
  }

  DictionaryValue* dict = static_cast<DictionaryValue*>(result);
  return dict->GetInteger("x", x) && dict->GetInteger("y", y);
}

bool WebElementCommand::GetElementSize(int* width, int* height) {
  scoped_ptr<ListValue> args(new ListValue());
  Value* result = NULL;

  std::string jscript = build_atom(GET_SIZE, sizeof GET_LOCATION);
  args->Append(GetElementIdAsDictionaryValue(element_id));

  ErrorCode error = session_->ExecuteScript(jscript, args.get(), &result);
  if (error != kSuccess) {
    LOG(ERROR) << "Javascript failed to execute" << std::endl;
    return false;
  }

  if (result == NULL || result->GetType() != Value::TYPE_DICTIONARY) {
    LOG(ERROR) << "Expected JavaScript atom to return "
               << "{width:number, height:number} dictionary." << std::endl;
    return false;
  }

  DictionaryValue* dict = static_cast<DictionaryValue*>(result);
  return dict->GetInteger("width", width) &&
         dict->GetInteger("height", height);
}

void ElementValueCommand::ExecuteGet(Response* const response) {
  Value* unscoped_result = NULL;
  ListValue args;
  std::string script = "return arguments[0]['value']";
  args.Append(GetElementIdAsDictionaryValue(element_id));
  ErrorCode code =
      session_->ExecuteScript(script, &args, &unscoped_result);
  scoped_ptr<Value> result(unscoped_result);
  if (code != kSuccess) {
    SET_WEBDRIVER_ERROR(response, "Failed to execute script", code);
    return;
  }
  if (!result->IsType(Value::TYPE_STRING) &&
      !result->IsType(Value::TYPE_NULL)) {
    SET_WEBDRIVER_ERROR(response,
                        "Result is not string or null type",
                        kInternalServerError);
    return;
  }
  response->set_status(kSuccess);
  response->set_value(result.release());
}

void ElementValueCommand::ExecutePost(Response* const response) {
  ListValue* key_list;
  if (!GetListParameter("value", &key_list)) {
    SET_WEBDRIVER_ERROR(response,
                        "Missing or invalid 'value' parameter",
                        kBadRequest);
    return;
  }
  // Flatten the given array of strings into one.
  string16 keys;
  for (size_t i = 0; i < key_list->GetSize(); ++i) {
    string16 keys_list_part;
    key_list->GetString(i, &keys_list_part);
    for (size_t j = 0; j < keys_list_part.size(); ++j) {
      if (CBU16_IS_SURROGATE(keys_list_part[j])) {
        SET_WEBDRIVER_ERROR(
            response,
            "ChromeDriver only supports characters in the BMP",
            kBadRequest);
        return;
      }
    }
    keys.append(keys_list_part);
  }

  ErrorCode code =
      session_->SendKeys(GetElementIdAsDictionaryValue(element_id), keys);
  if (code != kSuccess) {
    SET_WEBDRIVER_ERROR(response,
                        "Internal SendKeys error",
                        code);
    return;
  }
  response->set_status(kSuccess);
}

void ElementTextCommand::ExecuteGet(Response* const response) {
  Value* unscoped_result = NULL;
  ListValue args;
  // TODO(jleyba): Use a real javascript atom.
  std::string script =
      "function getText(element) {"
      "  if (element instanceof Text) {"
      "    return element.textContent.replace(/^\\s+|\\s+$/g, '');"
      "  }"
      "  var childrenText = '';"
      "  for (var i = 0; i < element.childNodes.length; i++) {"
      "    childrenText += getText(element.childNodes[i]);"
      "  }"
      "  return childrenText;"
      "}"
      "return getText(arguments[0]);";
  args.Append(GetElementIdAsDictionaryValue(element_id));
  ErrorCode code =
      session_->ExecuteScript(script, &args, &unscoped_result);
  scoped_ptr<Value> result(unscoped_result);
  if (code != kSuccess) {
    SET_WEBDRIVER_ERROR(response, "Failed to execute script", code);
    return;
  }
  if (!result->IsType(Value::TYPE_STRING)) {
    SET_WEBDRIVER_ERROR(response,
                        "Result is not string type",
                        kInternalServerError);
    return;
  }
  response->set_status(kSuccess);
  response->set_value(result.release());
}

}  // namespace webdriver
