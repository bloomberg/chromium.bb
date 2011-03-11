// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/webelement_commands.h"

#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/error_codes.h"
#include "chrome/test/webdriver/session.h"
#include "third_party/webdriver/atoms.h"
#include "ui/gfx/point.h"
#include "ui/gfx/size.h"

namespace webdriver {

///////////////////// WebElementCommand ////////////////////

WebElementCommand::WebElementCommand(
    const std::vector<std::string>& path_segments,
    const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters),
      path_segments_(path_segments) {}

WebElementCommand::~WebElementCommand() {}

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
  element = WebElementId(path_segments_.at(4));
  return true;
}

///////////////////// ElementAttributeCommand ////////////////////

ElementAttributeCommand::ElementAttributeCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementAttributeCommand::~ElementAttributeCommand() {}

bool ElementAttributeCommand::DoesGet() {
  return true;
}

void ElementAttributeCommand::ExecuteGet(Response* const response) {
  // There should be at least 7 segments to match
  // "/session/$session/element/$id/attribute/$name"
  if (path_segments_.size() < 7) {
    SET_WEBDRIVER_ERROR(response, "Path segments is less than 7",
                        kBadRequest);
    return;
  }

  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);", atoms::GET_ATTRIBUTE);

  scoped_ptr<ListValue> args(new ListValue);
  args->Append(element.ToValue());
  args->Append(Value::CreateStringValue(path_segments_.at(6)));

  Value* result = NULL;
  ErrorCode status = session_->ExecuteScript(script, args.get(), &result);
  response->SetStatus(status);
  response->SetValue(result);
}

///////////////////// ElementClearCommand ////////////////////

ElementClearCommand::ElementClearCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementClearCommand::~ElementClearCommand() {}

bool ElementClearCommand::DoesPost() {
  return true;
}

void ElementClearCommand::ExecutePost(Response* const response) {
  scoped_ptr<ListValue> args(new ListValue);
  args->Append(element.ToValue());

  std::string script = base::StringPrintf(
      "(%s).apply(null, arguments);", atoms::CLEAR);

  Value* result = NULL;
  ErrorCode status = session_->ExecuteScript(script, args.get(), &result);
  response->SetStatus(status);
  response->SetValue(result);
}

///////////////////// ElementCssCommand ////////////////////

ElementCssCommand::ElementCssCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementCssCommand::~ElementCssCommand() {}

bool ElementCssCommand::DoesGet() {
  return true;
}

void ElementCssCommand::ExecuteGet(Response* const response) {
  // There should be at least 7 segments to match
  // "/session/$session/element/$id/css/$propertyName"
  if (path_segments_.size() < 7) {
    SET_WEBDRIVER_ERROR(response, "Path segments is less than 7",
                        kBadRequest);
    return;
  }

  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);", atoms::GET_EFFECTIVE_STYLE);

  scoped_ptr<ListValue> args(new ListValue);
  args->Append(element.ToValue());
  args->Append(Value::CreateStringValue(path_segments_.at(6)));

  Value* result = NULL;
  ErrorCode status = session_->ExecuteScript(script, args.get(), &result);
  response->SetStatus(status);
  response->SetValue(result);
}

///////////////////// ElementDisplayedCommand ////////////////////

ElementDisplayedCommand::ElementDisplayedCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementDisplayedCommand::~ElementDisplayedCommand() {}

bool ElementDisplayedCommand::DoesGet() {
  return true;
}

void ElementDisplayedCommand::ExecuteGet(Response* const response) {
  scoped_ptr<ListValue> args(new ListValue);
  args->Append(element.ToValue());

  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);", atoms::IS_DISPLAYED);

  Value* result = NULL;
  ErrorCode status = session_->ExecuteScript(script, args.get(), &result);
  response->SetStatus(status);
  response->SetValue(result);
}

///////////////////// ElementEnabledCommand ////////////////////

ElementEnabledCommand::ElementEnabledCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementEnabledCommand::~ElementEnabledCommand() {}

bool ElementEnabledCommand::DoesGet() {
  return true;
}

void ElementEnabledCommand::ExecuteGet(Response* const response) {
  scoped_ptr<ListValue> args(new ListValue);
  args->Append(element.ToValue());

  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);", atoms::IS_ENABLED);

  Value* result = NULL;
  ErrorCode status = session_->ExecuteScript(script, args.get(), &result);
  response->SetStatus(status);
  response->SetValue(result);
}

///////////////////// ElementEqualsCommand ////////////////////

ElementEqualsCommand::ElementEqualsCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementEqualsCommand::~ElementEqualsCommand() {}

bool ElementEqualsCommand::DoesGet() {
  return true;
}

void ElementEqualsCommand::ExecuteGet(Response* const response) {
  // There should be at least 7 segments to match
  // "/session/$session/element/$id/equals/$other"
  if (path_segments_.size() < 7) {
    SET_WEBDRIVER_ERROR(response, "Path segments is less than 7",
                        kBadRequest);
    return;
  }

  std::string script = "return arguments[0] == arguments[1];";

  scoped_ptr<ListValue> args(new ListValue);
  args->Append(element.ToValue());
  args->Append(Value::CreateStringValue(path_segments_.at(6)));

  Value* result = NULL;
  ErrorCode status = session_->ExecuteScript(script, args.get(), &result);
  response->SetStatus(status);
  response->SetValue(result);
}

///////////////////// ElementLocationCommand ////////////////////

ElementLocationCommand::ElementLocationCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementLocationCommand::~ElementLocationCommand() {}

bool ElementLocationCommand::DoesGet() {
  return true;
}

void ElementLocationCommand::ExecuteGet(Response* const response) {
  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);", atoms::GET_LOCATION);

  ListValue args;
  args.Append(element.ToValue());

  Value* result = NULL;
  ErrorCode status = session_->ExecuteScript(script, &args, &result);
  response->SetStatus(status);
  response->SetValue(result);
}

///////////////////// ElementLocationInViewCommand ////////////////////

ElementLocationInViewCommand::ElementLocationInViewCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementLocationInViewCommand::~ElementLocationInViewCommand() {}

bool ElementLocationInViewCommand::DoesGet() {
  return true;
}

void ElementLocationInViewCommand::ExecuteGet(Response* const response) {
  gfx::Point location;
  ErrorCode code = session_->GetElementLocationInView(element, &location);
  response->SetStatus(code);
  if (code == kSuccess) {
    DictionaryValue* coord_dict = new DictionaryValue();
    coord_dict->SetInteger("x", location.x());
    coord_dict->SetInteger("y", location.y());
    response->SetValue(coord_dict);
  }
}

///////////////////// ElementNameCommand ////////////////////

ElementNameCommand::ElementNameCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementNameCommand::~ElementNameCommand() {}

bool ElementNameCommand::DoesGet() {
  return true;
}

void ElementNameCommand::ExecuteGet(Response* const response) {
  scoped_ptr<ListValue> args(new ListValue);
  args->Append(element.ToValue());

  std::string script = "return arguments[0].tagName;";

  Value* result = NULL;
  ErrorCode status = session_->ExecuteScript(script, args.get(), &result);
  response->SetStatus(status);
  response->SetValue(result);
}

///////////////////// ElementSelectedCommand ////////////////////

ElementSelectedCommand::ElementSelectedCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementSelectedCommand::~ElementSelectedCommand() {}

bool ElementSelectedCommand::DoesGet() {
  return true;
}

bool ElementSelectedCommand::DoesPost() {
  return true;
}

void ElementSelectedCommand::ExecuteGet(Response* const response) {
  scoped_ptr<ListValue> args(new ListValue);
  args->Append(element.ToValue());

  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);", atoms::IS_SELECTED);

  Value* result = NULL;
  ErrorCode status = session_->ExecuteScript(script, args.get(), &result);
  response->SetStatus(status);
  response->SetValue(result);
}

void ElementSelectedCommand::ExecutePost(Response* const response) {
  scoped_ptr<ListValue> args(new ListValue);
  args->Append(element.ToValue());
  args->Append(Value::CreateBooleanValue(true));

  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);", atoms::SET_SELECTED);

  Value* result = NULL;
  ErrorCode status = session_->ExecuteScript(script, args.get(), &result);
  response->SetStatus(status);
  response->SetValue(result);
}

///////////////////// ElementSizeCommand ////////////////////

ElementSizeCommand::ElementSizeCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementSizeCommand::~ElementSizeCommand() {}

bool ElementSizeCommand::DoesGet() {
  return true;
}

void ElementSizeCommand::ExecuteGet(Response* const response) {
  gfx::Size size;
  ErrorCode status = session_->GetElementSize(
      session_->current_target(), element, &size);
  if (status == kSuccess) {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("width", size.width());
    dict->SetInteger("height", size.height());
    response->SetValue(dict);
  }
  response->SetStatus(status);
}

///////////////////// ElementSubmitCommand ////////////////////

ElementSubmitCommand::ElementSubmitCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementSubmitCommand::~ElementSubmitCommand() {}

bool ElementSubmitCommand::DoesPost() {
  return true;
}

void ElementSubmitCommand::ExecutePost(Response* const response) {
  // TODO(jleyba): We need to wait for any post-submit navigation events to
  // complete before responding to the client.
  std::string script = base::StringPrintf(
      "(%s).apply(null, arguments);", atoms::SUBMIT);

  ListValue args;
  args.Append(element.ToValue());

  Value* result = NULL;
  ErrorCode status = session_->ExecuteScript(script, &args, &result);
  response->SetStatus(status);
  response->SetValue(result);
}

///////////////////// ElementToggleCommand ////////////////////

ElementToggleCommand::ElementToggleCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementToggleCommand::~ElementToggleCommand() {}

bool ElementToggleCommand::DoesPost() {
  return true;
}

void ElementToggleCommand::ExecutePost(Response* const response) {
  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);", atoms::TOGGLE);

  ListValue args;
  args.Append(element.ToValue());

  Value* result = NULL;
  ErrorCode status = session_->ExecuteScript(script, &args, &result);
  response->SetStatus(status);
  response->SetValue(result);
}

///////////////////// ElementValueCommand ////////////////////

ElementValueCommand::ElementValueCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementValueCommand::~ElementValueCommand() {}

bool ElementValueCommand::DoesGet() {
  return true;
}

bool ElementValueCommand::DoesPost() {
  return true;
}

void ElementValueCommand::ExecuteGet(Response* const response) {
  Value* unscoped_result = NULL;
  ListValue args;
  std::string script = "return arguments[0]['value']";
  args.Append(element.ToValue());
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
  response->SetStatus(kSuccess);
  response->SetValue(result.release());
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

  ErrorCode code = session_->SendKeys(element, keys);
  if (code != kSuccess) {
    SET_WEBDRIVER_ERROR(response,
                        "Internal SendKeys error",
                        code);
    return;
  }
  response->SetStatus(kSuccess);
}

///////////////////// ElementTextCommand ////////////////////

ElementTextCommand::ElementTextCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebElementCommand(path_segments, parameters) {}

ElementTextCommand::~ElementTextCommand() {}

bool ElementTextCommand::DoesGet() {
  return true;
}

void ElementTextCommand::ExecuteGet(Response* const response) {
  Value* unscoped_result = NULL;
  ListValue args;
  args.Append(element.ToValue());

  std::string script = base::StringPrintf(
      "return (%s).apply(null, arguments);", atoms::GET_TEXT);

  ErrorCode code = session_->ExecuteScript(script, &args,
                                           &unscoped_result);
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
  response->SetStatus(kSuccess);
  response->SetValue(result.release());
}

}  // namespace webdriver
