// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/webelement_commands.h"

#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_split.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/webdriver_basic_types.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_session.h"
#include "chrome/test/webdriver/webdriver_util.h"
#include "third_party/webdriver/atoms.h"

namespace webdriver {

///////////////////// WebElementCommand ////////////////////

WebElementCommand::WebElementCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters),
      path_segments_(path_segments) {}

WebElementCommand::~WebElementCommand() {}

bool WebElementCommand::Init(Response* const response) {
  if (!WebDriverCommand::Init(response))
    return false;

  // There should be at least 5 segments to match
  // "/session/$session/element/$id"
  if (path_segments_.size() < 5) {
    response->SetError(new Error(kBadRequest, "Path segments is less than 5"));
    return false;
  }

  // We cannot verify the ID is valid until we execute the command and
  // inject the ID into the in-page cache.
  element = ElementId(path_segments_.at(4));
  return true;
}

///////////////////// ElementAttributeCommand ////////////////////

ElementAttributeCommand::ElementAttributeCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementAttributeCommand::~ElementAttributeCommand() {}

bool ElementAttributeCommand::DoesGet() {
  return true;
}

void ElementAttributeCommand::ExecuteGet(Response* const response) {
  // There should be at least 7 segments to match
  // "/session/$session/element/$id/attribute/$name"
  if (path_segments_.size() < 7) {
    response->SetError(new Error(kBadRequest, "Path segments is less than 7"));
    return;
  }

  const std::string key = path_segments_.at(6);
  base::Value* value;
  Error* error = session_->GetAttribute(element, key, &value);
  if (error) {
    response->SetError(error);
    return;
  }

  response->SetValue(value);
}

///////////////////// ElementClearCommand ////////////////////

ElementClearCommand::ElementClearCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementClearCommand::~ElementClearCommand() {}

bool ElementClearCommand::DoesPost() {
  return true;
}

void ElementClearCommand::ExecutePost(Response* const response) {
  base::ListValue args;
  args.Append(element.ToValue());

  std::string script = base::StringPrintf(
      "(%s).apply(null, arguments);", atoms::asString(atoms::CLEAR).c_str());

  base::Value* result = NULL;
  Error* error = session_->ExecuteScript(script, &args, &result);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(result);
}

///////////////////// ElementCssCommand ////////////////////

ElementCssCommand::ElementCssCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementCssCommand::~ElementCssCommand() {}

bool ElementCssCommand::DoesGet() {
  return true;
}

void ElementCssCommand::ExecuteGet(Response* const response) {
  // There should be at least 7 segments to match
  // "/session/$session/element/$id/css/$propertyName"
  if (path_segments_.size() < 7) {
    response->SetError(new Error(kBadRequest, "Path segments is less than 7"));
    return;
  }

  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);",
      atoms::asString(atoms::GET_EFFECTIVE_STYLE).c_str());

  base::ListValue args;
  args.Append(element.ToValue());
  args.Append(new base::StringValue(path_segments_.at(6)));

  base::Value* result = NULL;
  Error* error = session_->ExecuteScript(script, &args, &result);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(result);
}

///////////////////// ElementDisplayedCommand ////////////////////

ElementDisplayedCommand::ElementDisplayedCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementDisplayedCommand::~ElementDisplayedCommand() {}

bool ElementDisplayedCommand::DoesGet() {
  return true;
}

void ElementDisplayedCommand::ExecuteGet(Response* const response) {
  bool is_displayed;
  Error* error = session_->IsElementDisplayed(
      session_->current_target(), element, false /* ignore_opacity */,
      &is_displayed);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(new base::FundamentalValue(is_displayed));
}

///////////////////// ElementEnabledCommand ////////////////////

ElementEnabledCommand::ElementEnabledCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementEnabledCommand::~ElementEnabledCommand() {}

bool ElementEnabledCommand::DoesGet() {
  return true;
}

void ElementEnabledCommand::ExecuteGet(Response* const response) {
  base::ListValue args;
  args.Append(element.ToValue());

  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);",
      atoms::asString(atoms::IS_ENABLED).c_str());

  base::Value* result = NULL;
  Error* error = session_->ExecuteScript(script, &args, &result);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(result);
}

///////////////////// ElementEqualsCommand ////////////////////

ElementEqualsCommand::ElementEqualsCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementEqualsCommand::~ElementEqualsCommand() {}

bool ElementEqualsCommand::DoesGet() {
  return true;
}

void ElementEqualsCommand::ExecuteGet(Response* const response) {
  // There should be at least 7 segments to match
  // "/session/$session/element/$id/equals/$other"
  if (path_segments_.size() < 7) {
    response->SetError(new Error(kBadRequest, "Path segments is less than 7"));
    return;
  }

  std::string script = "return arguments[0] == arguments[1];";

  base::ListValue args;
  args.Append(element.ToValue());

  ElementId other_element(path_segments_.at(6));
  args.Append(other_element.ToValue());

  base::Value* result = NULL;
  Error* error = session_->ExecuteScript(script, &args, &result);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(result);
}

///////////////////// ElementLocationCommand ////////////////////

ElementLocationCommand::ElementLocationCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementLocationCommand::~ElementLocationCommand() {}

bool ElementLocationCommand::DoesGet() {
  return true;
}

void ElementLocationCommand::ExecuteGet(Response* const response) {
  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);",
      atoms::asString(atoms::GET_LOCATION).c_str());

  base::ListValue args;
  args.Append(element.ToValue());

  base::Value* result = NULL;
  Error* error = session_->ExecuteScript(script, &args, &result);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(result);
}

///////////////////// ElementLocationInViewCommand ////////////////////

ElementLocationInViewCommand::ElementLocationInViewCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementLocationInViewCommand::~ElementLocationInViewCommand() {}

bool ElementLocationInViewCommand::DoesGet() {
  return true;
}

void ElementLocationInViewCommand::ExecuteGet(Response* const response) {
  Point location;
  Error* error = session_->GetElementLocationInView(element, &location);
  if (error) {
    response->SetError(error);
    return;
  }
  base::DictionaryValue* coord_dict = new base::DictionaryValue();
  coord_dict->SetInteger("x", location.x());
  coord_dict->SetInteger("y", location.y());
  response->SetValue(coord_dict);
}

///////////////////// ElementNameCommand ////////////////////

ElementNameCommand::ElementNameCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementNameCommand::~ElementNameCommand() {}

bool ElementNameCommand::DoesGet() {
  return true;
}

void ElementNameCommand::ExecuteGet(Response* const response) {
  std::string tag_name;
  Error* error = session_->GetElementTagName(
      session_->current_target(), element, &tag_name);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(new base::StringValue(tag_name));
}

///////////////////// ElementSelectedCommand ////////////////////

ElementSelectedCommand::ElementSelectedCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementSelectedCommand::~ElementSelectedCommand() {}

bool ElementSelectedCommand::DoesGet() {
  return true;
}

bool ElementSelectedCommand::DoesPost() {
  return true;
}

void ElementSelectedCommand::ExecuteGet(Response* const response) {
  bool is_selected;
  Error* error = session_->IsOptionElementSelected(
      session_->current_target(), element, &is_selected);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(new base::FundamentalValue(is_selected));
}

void ElementSelectedCommand::ExecutePost(Response* const response) {
  Error* error = session_->SetOptionElementSelected(
      session_->current_target(), element, true);
  if (error) {
    response->SetError(error);
    return;
  }
}

///////////////////// ElementSizeCommand ////////////////////

ElementSizeCommand::ElementSizeCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementSizeCommand::~ElementSizeCommand() {}

bool ElementSizeCommand::DoesGet() {
  return true;
}

void ElementSizeCommand::ExecuteGet(Response* const response) {
  Size size;
  Error* error = session_->GetElementSize(
      session_->current_target(), element, &size);
  if (error) {
    response->SetError(error);
    return;
  }
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("width", size.width());
  dict->SetInteger("height", size.height());
  response->SetValue(dict);
}

///////////////////// ElementSubmitCommand ////////////////////

ElementSubmitCommand::ElementSubmitCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementSubmitCommand::~ElementSubmitCommand() {}

bool ElementSubmitCommand::DoesPost() {
  return true;
}

void ElementSubmitCommand::ExecutePost(Response* const response) {
  std::string script = base::StringPrintf(
      "(%s).apply(null, arguments);", atoms::asString(atoms::SUBMIT).c_str());

  base::ListValue args;
  args.Append(element.ToValue());

  base::Value* result = NULL;
  Error* error = session_->ExecuteScript(script, &args, &result);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(result);
}

///////////////////// ElementToggleCommand ////////////////////

ElementToggleCommand::ElementToggleCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementToggleCommand::~ElementToggleCommand() {}

bool ElementToggleCommand::DoesPost() {
  return true;
}

void ElementToggleCommand::ExecutePost(Response* const response) {
  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);",
      atoms::asString(atoms::CLICK).c_str());

  base::ListValue args;
  args.Append(element.ToValue());

  base::Value* result = NULL;
  Error* error = session_->ExecuteScript(script, &args, &result);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(result);
}

///////////////////// ElementValueCommand ////////////////////

ElementValueCommand::ElementValueCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementValueCommand::~ElementValueCommand() {}

bool ElementValueCommand::DoesGet() {
  return true;
}

bool ElementValueCommand::DoesPost() {
  return true;
}

void ElementValueCommand::ExecuteGet(Response* const response) {
  base::Value* unscoped_result = NULL;
  base::ListValue args;
  std::string script = "return arguments[0]['value']";
  args.Append(element.ToValue());

  Error* error = session_->ExecuteScript(script, &args, &unscoped_result);
  scoped_ptr<base::Value> result(unscoped_result);
  if (error) {
    response->SetError(error);
    return;
  }
  if (!result->IsType(base::Value::TYPE_STRING) &&
      !result->IsType(base::Value::TYPE_NULL)) {
    response->SetError(new Error(
        kUnknownError, "Result is not string or null type"));
    return;
  }
  response->SetValue(result.release());
}

void ElementValueCommand::ExecutePost(Response* const response) {
  bool is_input = false;
  Error* error = HasAttributeWithLowerCaseValueASCII("tagName", "input",
                                                     &is_input);
  if (error) {
    response->SetError(error);
    return;
  }

  bool is_file = false;
  error = HasAttributeWithLowerCaseValueASCII("type", "file", &is_file);
  if (error) {
    response->SetError(error);
    return;
  }

  // If the element is a file upload control, set the file paths to the element.
  // Otherwise send the value to the element as key input.
  if (is_input && is_file) {
    error = DragAndDropFilePaths();
  } else {
    error = SendKeys();
  }

  if (error) {
    response->SetError(error);
    return;
  }
}

Error* ElementValueCommand::HasAttributeWithLowerCaseValueASCII(
    const std::string& key, const std::string& value, bool* result) const {
  base::Value* unscoped_value = NULL;
  Error* error = session_->GetAttribute(element, key, &unscoped_value);
  scoped_ptr<base::Value> scoped_value(unscoped_value);
  if (error)
    return error;

  std::string actual_value;
  if (scoped_value->GetAsString(&actual_value)) {
    *result = LowerCaseEqualsASCII(actual_value, value.c_str());
  } else {
    // Note we do not handle converting a number to a string.
    *result = false;
  }
  return NULL;
}

Error* ElementValueCommand::DragAndDropFilePaths() const {
  const base::ListValue* path_list;
  if (!GetListParameter("value", &path_list))
    return new Error(kBadRequest, "Missing or invalid 'value' parameter");

  // Compress array into single string.
  base::FilePath::StringType paths_string;
  for (size_t i = 0; i < path_list->GetSize(); ++i) {
    base::FilePath::StringType path_part;
    if (!path_list->GetString(i, &path_part)) {
      return new Error(
          kBadRequest,
          "'value' is invalid: " + JsonStringify(path_list));
    }
    paths_string.append(path_part);
  }

  // Separate the string into separate paths, delimited by \n.
  std::vector<base::FilePath::StringType> paths;
  base::SplitString(paths_string, '\n', &paths);

  // Return an error if trying to drop multiple paths on a single file input.
  bool multiple = false;
  Error* error = HasAttributeWithLowerCaseValueASCII("multiple", "true",
                                                     &multiple);
  if (error)
    return error;
  if (!multiple && paths.size() > 1)
    return new Error(kBadRequest, "The element can not hold multiple files");

  // Check the files exist.
  for (size_t i = 0; i < paths.size(); ++i) {
    if (!file_util::PathExists(base::FilePath(paths[i]))) {
      return new Error(
          kBadRequest,
          base::StringPrintf("'%s' does not exist on the file system",
              UTF16ToUTF8(
                  base::FilePath(paths[i]).LossyDisplayName()).c_str()));
    }
  }

  Point location;
  error = session_->GetClickableLocation(element, &location);
  if (error)
    return error;

  return session_->DragAndDropFilePaths(location, paths);
}

Error* ElementValueCommand::SendKeys() const {
  const base::ListValue* key_list;
  if (!GetListParameter("value", &key_list)) {
    return new Error(kBadRequest, "Missing or invalid 'value' parameter");
  }

  // Flatten the given array of strings into one.
  string16 keys;
  Error* error = FlattenStringArray(key_list, &keys);
  if (error)
    return error;

  return session_->SendKeys(element, keys);
}

///////////////////// ElementTextCommand ////////////////////

ElementTextCommand::ElementTextCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementTextCommand::~ElementTextCommand() {}

bool ElementTextCommand::DoesGet() {
  return true;
}

void ElementTextCommand::ExecuteGet(Response* const response) {
  base::Value* unscoped_result = NULL;
  base::ListValue args;
  args.Append(element.ToValue());

  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);",
      atoms::asString(atoms::GET_TEXT).c_str());

  Error* error = session_->ExecuteScript(script, &args,
                                         &unscoped_result);
  scoped_ptr<base::Value> result(unscoped_result);
  if (error) {
    response->SetError(error);
    return;
  }
  if (!result->IsType(base::Value::TYPE_STRING)) {
    response->SetError(new Error(kUnknownError, "Result is not string type"));
    return;
  }
  response->SetValue(result.release());
}

}  // namespace webdriver
