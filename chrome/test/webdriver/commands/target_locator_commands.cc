// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/target_locator_commands.h"

#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/web_element_id.h"
#include "chrome/test/webdriver/webdriver_error.h"

namespace webdriver {

WindowHandleCommand::WindowHandleCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {}

WindowHandleCommand::~WindowHandleCommand() {}

bool WindowHandleCommand::DoesGet() {
  return true;
}

void WindowHandleCommand::ExecuteGet(Response* const response) {
  response->SetValue(new StringValue(
      base::IntToString(session_->current_target().window_id)));
}

WindowHandlesCommand::WindowHandlesCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {}

WindowHandlesCommand::~WindowHandlesCommand() {}

bool WindowHandlesCommand::DoesGet() {
  return true;
}

void WindowHandlesCommand::ExecuteGet(Response* const response) {
  std::vector<int> window_ids;
  Error* error = session_->GetWindowIds(&window_ids);
  if (error) {
    response->SetError(error);
    return;
  }
  ListValue* id_list = new ListValue();
  for (size_t i = 0; i < window_ids.size(); ++i)
    id_list->Append(new StringValue(base::IntToString(window_ids[i])));
  response->SetValue(id_list);
}

WindowCommand::WindowCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {}

WindowCommand::~WindowCommand() {}

bool WindowCommand::DoesPost() {
  return true;
}

bool WindowCommand::DoesDelete() {
  return true;
}

void WindowCommand::ExecutePost(Response* const response) {
  std::string name;
  if (!GetStringParameter("name", &name)) {
    response->SetError(new Error(
        kBadRequest, "Missing or invalid 'name' parameter"));
    return;
  }

  Error* error = session_->SwitchToWindow(name);
  if (error)
    response->SetError(error);
}

void WindowCommand::ExecuteDelete(Response* const response) {
  Error* error = session_->CloseWindow();
  if (error)
    response->SetError(error);
}

SwitchFrameCommand::SwitchFrameCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {}

SwitchFrameCommand::~SwitchFrameCommand() {}

bool SwitchFrameCommand::DoesPost() {
  return true;
}

void SwitchFrameCommand::ExecutePost(Response* const response) {
  std::string id;
  int index = 0;
  WebElementId element;
  Error* error = NULL;
  if (GetStringParameter("id", &id)) {
    error = session_->SwitchToFrameWithNameOrId(id);
  } else if (GetIntegerParameter("id", &index)) {
    error = session_->SwitchToFrameWithIndex(index);
  } else if (GetWebElementParameter("id", &element)) {
    error = session_->SwitchToFrameWithElement(element);
  } else if (IsNullParameter("id") || !HasParameter("id")) {
    // Treat null 'id' and no 'id' as the same.
    // See http://code.google.com/p/selenium/issues/detail?id=1479.
    session_->SwitchToTopFrame();
  } else {
    error = new Error(kBadRequest, "Invalid 'id' parameter");
  }
  if (error)
    response->SetError(error);
}

bool SwitchFrameCommand::GetWebElementParameter(const std::string& key,
                                                WebElementId* out) const {
  DictionaryValue* value;
  if (!GetDictionaryParameter(key, &value))
    return false;

  WebElementId id(value);
  if (!id.is_valid())
    return false;

  *out = id;
  return true;
}

ActiveElementCommand::ActiveElementCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {}

ActiveElementCommand::~ActiveElementCommand() {}

bool ActiveElementCommand::DoesPost() {
  return true;
}

void ActiveElementCommand::ExecutePost(Response* const response) {
  ListValue args;
  Value* result = NULL;
  Error* error = session_->ExecuteScript(
      "return document.activeElement || document.body", &args, &result);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(result);
}

}  // namespace webdriver
