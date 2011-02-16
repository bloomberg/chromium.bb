// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/find_element_commands.h"

#include <sstream>
#include <string>

#include "base/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "third_party/webdriver/atoms.h"
#include "chrome/test/webdriver/error_codes.h"

namespace webdriver {

FindElementCommand::FindElementCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters,
    const bool find_one_element)
    : WebDriverCommand(path_segments, parameters),
      find_one_element_(find_one_element) {}

FindElementCommand::~FindElementCommand() {}

bool FindElementCommand::Init(Response* const response) {
  if (!WebDriverCommand::Init(response)) {
    SET_WEBDRIVER_ERROR(response, "Failure on Init for find element",
                        kInternalServerError);
    return false;
  }

  if (!GetStringASCIIParameter("using", &use_) ||
      !GetStringASCIIParameter("value", &value_)) {
    SET_WEBDRIVER_ERROR(response,
        "Request is missing required 'using' and/or 'value' data", kBadRequest);
    return false;
  }

  // TODO(jmikhail): The findElement(s) atom should handle this conversion.
  if ("class name" == use_) {
    use_ = "className";
  } else if ("link text" == use_) {
    use_ = "linkText";
  } else if ("partial link text" == use_) {
    use_ = "partialLinkText";
  } else if ("tag name" == use_) {
    use_ = "tagName";
  }

  // Searching under a custom root if the URL pattern is
  // "/session/$session/element/$id/element(s)"
  root_element_id_ = GetPathVariable(4);

  return true;
}

bool FindElementCommand::DoesPost() {
  return true;
}

void FindElementCommand::ExecutePost(Response* const response) {
  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* locator = new DictionaryValue();
  ErrorCode error;
  std::string jscript;
  Value* result = NULL;
  bool done = false;

  // Set the command we are using to locate the value beging searched for.
  locator->SetString(use_, value_);
  args->Append(locator);
  args->Append(root_element_id_.size() == 0 ? Value::CreateNullValue() :
      WebDriverCommand::GetElementIdAsDictionaryValue(root_element_id_));
  if (find_one_element_) {
    // TODO(jleyba): Write a Chrome-specific find element atom that will
    // correctly throw an error if the element cannot be found.
    jscript = base::StringPrintf(
        "var result = (%s).apply(null, arguments);"
        "if (!result) {"
        "var e = new Error('Unable to locate element');"
        "e.code = %d;"
        "throw e;"
        "} else { return result; }",
        atoms::FIND_ELEMENT, kNoSuchElement);
  } else {
    jscript = base::StringPrintf("return (%s).apply(null, arguments);",
                                 atoms::FIND_ELEMENTS);
  }

  // The element search needs to loop until at least one element is found or the
  // session's implicit wait timeout expires, whichever occurs first.
  base::Time start_time = base::Time::Now();

  while (!done) {
    if (result) {
      delete result;
      result = NULL;
    }

    error = session_->ExecuteScript(jscript, args.get(), &result);
    if (error == kSuccess) {
      // If searching for many elements, make sure we found at least one before
      // stopping.
      done = find_one_element_ ||
             (result->GetType() == Value::TYPE_LIST &&
             static_cast<ListValue*>(result)->GetSize() > 0);
    } else if (error != kNoSuchElement) {
      SET_WEBDRIVER_ERROR(response, "Internal error in find_element atom",
                          kInternalServerError);
      return;
    }

    int64 elapsed_time = (base::Time::Now() - start_time).InMilliseconds();
    done = done || elapsed_time > session_->implicit_wait();
    base::PlatformThread::Sleep(50);  // Prevent a busy loop that eats the cpu.
  }

  response->set_value(result);
  response->set_status(error);
}

bool FindElementCommand::RequiresValidTab() {
  return false;
}

}  // namespace webdriver
