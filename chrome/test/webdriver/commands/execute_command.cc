// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/json/json_reader.h"

#include "chrome/test/webdriver/commands/execute_command.h"

namespace webdriver {

const char kArgs[] = "args";
const char kScript[] = "script";

bool ExecuteCommand::Init(Response* const response) {
  if (!WebDriverCommand::Init(response)) {
    SET_WEBDRIVER_ERROR(response, "Failure on Init for execute command",
                        kInternalServerError);
    return false;
  }

  if (!GetStringParameter(kScript, &script_)) {
    SET_WEBDRIVER_ERROR(response, "No script to execute specified",
                        kBadRequest);
    return false;
  }

  has_args_= GetStringASCIIParameter(kArgs, &args_);
  return true;
}

void ExecuteCommand::ExecutePost(Response* const response) {
  int error_code = 0;
  std::string error_msg;
  Value* params = NULL;
  Value* result = NULL;

  if (has_args_) {
    params = base::JSONReader::ReadAndReturnError(args_, true,
                                                  &error_code, &error_msg);
    if (error_code != 0) {
      LOG(INFO) << "Could not parse the JSON arguments passed in "
                << "got error code: " <<  error_code << ", " << error_msg;
      SET_WEBDRIVER_ERROR(response, "Arguments are not valid json objects",
                          kBadRequest);
      return;
    }
  } else {
    LOG(INFO) << "No args for this script";
    // If there are no args required just use an empty list.
    params = new ListValue();
  }

  if (params->GetType() != Value::TYPE_LIST) {
    LOG(INFO) << "Data passed in to script must be a json list";
    SET_WEBDRIVER_ERROR(response, "Arguments are not in list format",
                        kBadRequest);
    return;
  }

  ListValue* script_args = static_cast<ListValue*>(params);
  ErrorCode error = session_->ExecuteScript(script_,
                                            script_args, &result);

  if (error != kSuccess) {
    SET_WEBDRIVER_ERROR(response, "Failed to execute script",
                        kInternalServerError);
    return;
  }

  response->set_value(result);
  response->set_status(kSuccess);
}

}  // namspace webdriver

