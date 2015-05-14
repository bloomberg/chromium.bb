// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/webui/proximity_auth_webui_handler.h"

#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"

namespace proximity_auth {

namespace {

// Keys in the JSON representation of a log message.
const char kLogMessageTextKey[] = "text";
const char kLogMessageTimeKey[] = "time";
const char kLogMessageFileKey[] = "file";
const char kLogMessageLineKey[] = "line";
const char kLogMessageSeverityKey[] = "severity";

// Converts |log_message| to a raw dictionary value used as a JSON argument to
// JavaScript functions.
scoped_ptr<base::DictionaryValue> LogMessageToDictionary(
    const LogBuffer::LogMessage& log_message) {
  scoped_ptr<base::DictionaryValue> dictionary(new base::DictionaryValue());
  dictionary->SetString(kLogMessageTextKey, log_message.text);
  dictionary->SetString(kLogMessageTimeKey,
                        base::TimeFormatTimeOfDay(log_message.time));
  dictionary->SetString(kLogMessageFileKey, log_message.file);
  dictionary->SetInteger(kLogMessageLineKey, log_message.line);
  dictionary->SetInteger(kLogMessageSeverityKey,
                         static_cast<int>(log_message.severity));
  return dictionary.Pass();
}

}  // namespace

ProximityAuthWebUIHandler::ProximityAuthWebUIHandler() {
  LogBuffer::GetInstance()->AddObserver(this);
}

ProximityAuthWebUIHandler::~ProximityAuthWebUIHandler() {
  LogBuffer::GetInstance()->RemoveObserver(this);
}

void ProximityAuthWebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "clearLogBuffer", base::Bind(&ProximityAuthWebUIHandler::ClearLogBuffer,
                                   base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getLogMessages", base::Bind(&ProximityAuthWebUIHandler::GetLogMessages,
                                   base::Unretained(this)));
}

void ProximityAuthWebUIHandler::OnLogMessageAdded(
    const LogBuffer::LogMessage& log_message) {
  scoped_ptr<base::DictionaryValue> dictionary =
      LogMessageToDictionary(log_message);
  web_ui()->CallJavascriptFunction("LogBufferInterface.onLogMessageAdded",
                                   *dictionary);
}

void ProximityAuthWebUIHandler::OnLogBufferCleared() {
  web_ui()->CallJavascriptFunction("LogBufferInterface.onLogBufferCleared");
}

void ProximityAuthWebUIHandler::GetLogMessages(const base::ListValue* args) {
  base::ListValue json_logs;
  auto logs = LogBuffer::GetInstance()->logs();
  std::transform(logs->begin(), logs->end(), json_logs.begin(),
                 [](const LogBuffer::LogMessage& log) {
                   return LogMessageToDictionary(log).release();
                 });

  web_ui()->CallJavascriptFunction("LogBufferInterface.onGotLogMessages",
                                   json_logs);
}

void ProximityAuthWebUIHandler::ClearLogBuffer(const base::ListValue* args) {
  // The OnLogBufferCleared() observer function will be called after the buffer
  // is cleared.
  LogBuffer::GetInstance()->Clear();
}

}  // namespace proximity_auth
