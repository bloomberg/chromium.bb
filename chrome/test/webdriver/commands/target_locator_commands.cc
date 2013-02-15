// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/target_locator_commands.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/webdriver_element_id.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_session.h"
#include "chrome/test/webdriver/webdriver_util.h"

namespace webdriver {

WindowHandleCommand::WindowHandleCommand(
    const std::vector<std::string>& path_segments,
    base::DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {}

WindowHandleCommand::~WindowHandleCommand() {}

bool WindowHandleCommand::DoesGet() {
  return true;
}

void WindowHandleCommand::ExecuteGet(Response* const response) {
  response->SetValue(new base::StringValue(
      WebViewIdToString(session_->current_target().view_id)));
}

WindowHandlesCommand::WindowHandlesCommand(
    const std::vector<std::string>& path_segments,
    base::DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {}

WindowHandlesCommand::~WindowHandlesCommand() {}

bool WindowHandlesCommand::DoesGet() {
  return true;
}

void WindowHandlesCommand::ExecuteGet(Response* const response) {
  std::vector<WebViewInfo> views;
  Error* error = session_->GetViews(&views);
  if (error) {
    response->SetError(error);
    return;
  }
  base::ListValue* id_list = new base::ListValue();
  for (size_t i = 0; i < views.size(); ++i) {
    if (!views[i].view_id.IsTab() &&
        views[i].view_id.GetId().type() != AutomationId::kTypeAppShell)
      continue;
    id_list->Append(new base::StringValue(WebViewIdToString(views[i].view_id)));
  }
  response->SetValue(id_list);
}

WindowCommand::WindowCommand(
    const std::vector<std::string>& path_segments,
    base::DictionaryValue* parameters)
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

  Error* error = session_->SwitchToView(name);
  if (error)
    response->SetError(error);
}

void WindowCommand::ExecuteDelete(Response* const response) {
  Error* error = session_->CloseWindow();
  if (error)
    response->SetError(error);
}

bool WindowCommand::ShouldRunPreAndPostCommandHandlers() {
  return false;
}

SwitchFrameCommand::SwitchFrameCommand(
    const std::vector<std::string>& path_segments,
    base::DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {}

SwitchFrameCommand::~SwitchFrameCommand() {}

bool SwitchFrameCommand::DoesPost() {
  return true;
}

void SwitchFrameCommand::ExecutePost(Response* const response) {
  std::string id;
  int index = 0;
  ElementId element;
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
                                                ElementId* out) const {
  const base::DictionaryValue* value;
  if (!GetDictionaryParameter(key, &value))
    return false;

  ElementId id(value);
  if (!id.is_valid())
    return false;

  *out = id;
  return true;
}

ActiveElementCommand::ActiveElementCommand(
    const std::vector<std::string>& path_segments,
    base::DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {}

ActiveElementCommand::~ActiveElementCommand() {}

bool ActiveElementCommand::DoesPost() {
  return true;
}

void ActiveElementCommand::ExecutePost(Response* const response) {
  base::ListValue args;
  base::Value* result = NULL;
  Error* error = session_->ExecuteScript(
      "return document.activeElement || document.body", &args, &result);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(result);
}

}  // namespace webdriver
