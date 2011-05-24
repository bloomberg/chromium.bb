// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/create_session.h"

#include <sstream>
#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/session_manager.h"
#include "chrome/test/webdriver/webdriver_error.h"

namespace webdriver {

CreateSession::CreateSession(const std::vector<std::string>& path_segments,
                             const DictionaryValue* const parameters)
    : Command(path_segments, parameters) {}

CreateSession::~CreateSession() {}

bool CreateSession::DoesPost() { return true; }

void CreateSession::ExecutePost(Response* const response) {
  DictionaryValue *capabilities = NULL;
  if (!GetDictionaryParameter("desiredCapabilities", &capabilities)) {
    response->SetError(new Error(
        kBadRequest, "Missing or invalid 'desiredCapabilities'"));
    return;
  }

  CommandLine command_line_options(CommandLine::NO_PROGRAM);
  ListValue* switches = NULL;
  const char* kCustomSwitchesKey = "chrome.switches";
  if (capabilities->GetListWithoutPathExpansion(kCustomSwitchesKey,
                                                &switches)) {
    for (size_t i = 0; i < switches->GetSize(); ++i) {
      std::string switch_string;
      if (!switches->GetString(i, &switch_string)) {
        response->SetError(new Error(
            kBadRequest, "Custom switch is not a string"));
        return;
      }
      size_t separator_index = switch_string.find("=");
      if (separator_index != std::string::npos) {
        CommandLine::StringType switch_string_native;
        if (!switches->GetString(i, &switch_string_native)) {
          response->SetError(new Error(
              kBadRequest, "Custom switch is not a string"));
          return;
        }
        command_line_options.AppendSwitchNative(
            switch_string.substr(0, separator_index),
            switch_string_native.substr(separator_index + 1));
      } else {
        command_line_options.AppendSwitch(switch_string);
      }
    }
  } else if (capabilities->HasKey(kCustomSwitchesKey)) {
    response->SetError(new Error(
        kBadRequest, "Custom switches must be a list"));
    return;
  }

  FilePath browser_exe;
  FilePath::StringType path;
  if (capabilities->GetStringWithoutPathExpansion("chrome.binary", &path))
    browser_exe = FilePath(path);

  // Session manages its own liftime, so do not call delete.
  Session* session = new Session();
  Error* error = session->Init(browser_exe, command_line_options);
  if (error) {
    response->SetError(error);
    return;
  }

  bool native_events_required = false;
  Value* native_events_value = NULL;
  if (capabilities->GetWithoutPathExpansion(
          "chrome.nativeEvents", &native_events_value)) {
    if (native_events_value->GetAsBoolean(&native_events_required)) {
      session->set_use_native_events(native_events_required);
    }
  }
  bool screenshot_on_error = false;
  if (capabilities->GetBoolean(
          "takeScreenshotOnError", &screenshot_on_error)) {
    session->set_screenshot_on_error(screenshot_on_error);
  }

  VLOG(1) << "Created session " << session->id();
  std::ostringstream stream;
  SessionManager* session_manager = SessionManager::GetInstance();
  stream << "http://" << session_manager->GetAddress() << "/session/"
         << session->id();
  response->SetStatus(kSeeOther);
  response->SetValue(Value::CreateStringValue(stream.str()));
}

}  // namespace webdriver
