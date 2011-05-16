// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/alert_commands.h"

#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/error_codes.h"
#include "chrome/test/webdriver/session.h"

namespace webdriver {

AlertTextCommand::AlertTextCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
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
  ErrorCode code = session_->GetAlertMessage(&text);
  if (code == kSuccess)
    response->SetValue(Value::CreateStringValue(text));
  response->SetStatus(code);
}

void AlertTextCommand::ExecutePost(Response* const response) {
  std::string text;
  if (!GetStringParameter("text", &text)) {
    SET_WEBDRIVER_ERROR(response, "'text' is missing or invalid", kBadRequest);
    return;
  }
  response->SetStatus(session_->SetAlertPromptText(text));
}

AcceptAlertCommand::AcceptAlertCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {
}

AcceptAlertCommand::~AcceptAlertCommand() {
}

bool AcceptAlertCommand::DoesPost() {
  return true;
}

void AcceptAlertCommand::ExecutePost(Response* const response) {
  response->SetStatus(session_->AcceptOrDismissAlert(true));
}

DismissAlertCommand::DismissAlertCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {
}

DismissAlertCommand::~DismissAlertCommand() {
}

bool DismissAlertCommand::DoesPost() {
  return true;
}

void DismissAlertCommand::ExecutePost(Response* const response) {
  response->SetStatus(session_->AcceptOrDismissAlert(false));
}

}  // namespace webdriver
