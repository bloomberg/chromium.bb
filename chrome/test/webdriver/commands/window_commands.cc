// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/window_commands.h"

#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_session.h"
#include "chrome/test/webdriver/webdriver_util.h"

using base::Value;

namespace webdriver {

namespace {

bool GetWindowId(const std::string& window_id_string,
                 const WebViewId& current_id,
                 WebViewId* window_id,
                 Response* const response) {
  if (window_id_string == "current") {
    *window_id = current_id;
  } else if (!StringToWebViewId(window_id_string, window_id)) {
    response->SetError(
        new Error(kBadRequest, "Invalid window ID"));
    return false;
  }
  return true;
}

}

WindowSizeCommand::WindowSizeCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {
}

WindowSizeCommand::~WindowSizeCommand() {
}

bool WindowSizeCommand::DoesGet() {
  return true;
}

bool WindowSizeCommand::DoesPost() {
  return true;
}

void WindowSizeCommand::ExecuteGet(Response* const response) {
  // Path segment: "/session/$sessionId/window/$windowHandle/size"
  WebViewId window_id;
  WebViewId current_id = session_->current_target().view_id;
  if (!GetWindowId(GetPathVariable(4), current_id, &window_id, response))
    return;

  Rect bounds;
  Error* error = session_->GetWindowBounds(window_id, &bounds);
  if (error) {
    response->SetError(error);
    return;
  }
  base::DictionaryValue* size = new base::DictionaryValue();
  size->SetInteger("width", bounds.width());
  size->SetInteger("height", bounds.height());
  response->SetValue(size);
}

void WindowSizeCommand::ExecutePost(Response* const response) {
  // Path segment: "/session/$sessionId/window/$windowHandle/size"
  WebViewId window_id;
  WebViewId current_id = session_->current_target().view_id;
  if (!GetWindowId(GetPathVariable(4), current_id, &window_id, response))
    return;

  int width, height;
  if (!GetIntegerParameter("width", &width) ||
      !GetIntegerParameter("height", &height)) {
    response->SetError(new Error(
        kBadRequest,
        "Missing or invalid 'width' or 'height' parameters"));
    return;
  }
  Rect bounds;
  Error* error = session_->GetWindowBounds(window_id, &bounds);
  if (!error) {
    bounds = Rect(bounds.origin(), Size(width, height));
    error = session_->SetWindowBounds(window_id, bounds);
  }
  if (error) {
    response->SetError(error);
    return;
  }
}

WindowPositionCommand::WindowPositionCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {
}

WindowPositionCommand::~WindowPositionCommand() {
}

bool WindowPositionCommand::DoesGet() {
  return true;
}

bool WindowPositionCommand::DoesPost() {
  return true;
}

void WindowPositionCommand::ExecuteGet(Response* const response) {
  // Path segment: "/session/$sessionId/window/$windowHandle/position"
  WebViewId window_id;
  WebViewId current_id = session_->current_target().view_id;
  if (!GetWindowId(GetPathVariable(4), current_id, &window_id, response))
    return;

  Rect bounds;
  Error* error = session_->GetWindowBounds(window_id, &bounds);
  if (error) {
    response->SetError(error);
    return;
  }
  base::DictionaryValue* size = new base::DictionaryValue();
  size->SetInteger("x", bounds.x());
  size->SetInteger("y", bounds.y());
  response->SetValue(size);
}

void WindowPositionCommand::ExecutePost(Response* const response) {
  // Path segment: "/session/$sessionId/window/$windowHandle/position"
  WebViewId window_id;
  WebViewId current_id = session_->current_target().view_id;
  if (!GetWindowId(GetPathVariable(4), current_id, &window_id, response))
    return;

  int x, y;
  if (!GetIntegerParameter("x", &x) ||
      !GetIntegerParameter("y", &y)) {
    response->SetError(new Error(
        kBadRequest,
        "Missing or invalid 'x' or 'y' parameters"));
    return;
  }
  Rect bounds;
  Error* error = session_->GetWindowBounds(window_id, &bounds);
  if (!error) {
    bounds = Rect(Point(x, y), bounds.size());
    error = session_->SetWindowBounds(window_id, bounds);
  }
  if (error) {
    response->SetError(error);
    return;
  }
}

WindowMaximizeCommand::WindowMaximizeCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {
}

WindowMaximizeCommand::~WindowMaximizeCommand() {
}

bool WindowMaximizeCommand::DoesPost() {
  return true;
}

void WindowMaximizeCommand::ExecutePost(Response* const response) {
  // Path segment: "/session/$sessionId/window/$windowHandle/maximize"
  WebViewId window_id;
  WebViewId current_id = session_->current_target().view_id;
  if (!GetWindowId(GetPathVariable(4), current_id, &window_id, response))
    return;

  Error* error = session_->MaximizeWindow(window_id);
  if (error) {
    response->SetError(error);
    return;
  }
}

}  // namespace webdriver
