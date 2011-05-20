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
  DictionaryValue *added_options = NULL, *desiredCapabilities = NULL;
  CommandLine options(CommandLine::NO_PROGRAM);
  FilePath user_browser_exe;

  if (!GetDictionaryParameter("desiredCapabilities", &desiredCapabilities)) {
    desiredCapabilities = NULL;
  }

  if ((desiredCapabilities != NULL) &&
      desiredCapabilities->GetDictionary("chrome.customSwitches",
                                         &added_options)) {
    DictionaryValue::key_iterator i = added_options->begin_keys();
    while (i != added_options->end_keys()) {
      FilePath::StringType value;
      if (!added_options->GetString(*i, &value)) {
        response->SetError(new Error(
            kBadRequest, "Invalid format for added options to browser"));
        return;
      } else {
        if (!value.empty()) {
          options.AppendSwitchNative(*i, value);
        } else {
          options.AppendSwitch(*i);
        }
      }
      ++i;
    }
    FilePath::StringType path;
    desiredCapabilities->GetString("chrome.binary", &path);
    user_browser_exe = FilePath(path);
  }

  // Session manages its own liftime, so do not call delete.
  Session* session = new Session();
  Error* error = session->Init(user_browser_exe, options);
  if (error) {
    response->SetError(error);
    return;
  }

  if (desiredCapabilities != NULL) {
    bool native_events_required = false;
    if (desiredCapabilities->GetBoolean("chrome.nativeEvents",
                                        &native_events_required)) {
      session->set_use_native_events(native_events_required);
    }

    bool screenshot_on_error = false;
    if (desiredCapabilities->GetBoolean("takeScreenshotOnError",
                                        &screenshot_on_error)) {
      session->set_screenshot_on_error(screenshot_on_error);
    }
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
