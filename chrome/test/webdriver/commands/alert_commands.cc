// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/alert_commands.h"

#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_session.h"

namespace webdriver {

AlertTextCommand::AlertTextCommand(
    const std::vector<std::string>& path_segments,
    base::DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {
}

AlertTextCommand::~AlertTextCommand() {
}

bool AlertTextCommand::DoesGet() {
  return true;
}

bool AlertTextCommand::DoesPost() {
  return true;
}

void AlertTextCommand::ExecuteGet(Response* const response) {
  std::string text;
  Error* error = session_->GetAlertMessage(&text);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(new base::StringValue(text));
}

void AlertTextCommand::ExecutePost(Response* const response) {
  std::string text;
  if (!GetStringParameter("text", &text)) {
    response->SetError(new Error(
        kBadRequest, "'text' is missing or invalid"));
    return;
  }
  Error* error = session_->SetAlertPromptText(text);
  if (error)
    response->SetError(error);
}

AcceptAlertCommand::AcceptAlertCommand(
    const std::vector<std::string>& path_segments,
    base::DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {
}

AcceptAlertCommand::~AcceptAlertCommand() {
}

bool AcceptAlertCommand::DoesPost() {
  return true;
}

void AcceptAlertCommand::ExecutePost(Response* const response) {
  Error* error = session_->AcceptOrDismissAlert(true);
  if (error)
    response->SetError(error);
}

DismissAlertCommand::DismissAlertCommand(
    const std::vector<std::string>& path_segments,
    base::DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {
}

DismissAlertCommand::~DismissAlertCommand() {
}

bool DismissAlertCommand::DoesPost() {
  return true;
}

void DismissAlertCommand::ExecutePost(Response* const response) {
  Error* error = session_->AcceptOrDismissAlert(false);
  if (error)
    response->SetError(error);
}

}  // namespace webdriver
