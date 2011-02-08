// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/webelement_command.h"

#include "third_party/webdriver/atoms.h"
#include "chrome/test/webdriver/error_codes.h"
#include "chrome/test/webdriver/utility_functions.h"

namespace webdriver {

bool WebElementCommand::Init(Response* const response) {
  if (WebDriverCommand::Init(response)) {
    SET_WEBDRIVER_ERROR(response, "Failure on Init for web element command",
                        kInternalServerError);
    return false;
  }

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

}  // namespace webdriver
