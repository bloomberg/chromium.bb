// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/webdriver_command.h"

#include <string>

#include "base/logging.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/values.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/automation/window_proxy.h"
#include "chrome/test/webdriver/utility_functions.h"
#include "views/event.h"

namespace webdriver {

const std::string WebDriverCommand::kElementDictionaryKey = "ELEMENT";

bool WebDriverCommand::IsElementIdDictionary(
    const DictionaryValue* const dictionary) {
  // Test that it has the element key and that it is a string.
  Value* element_id;
  return dictionary->Get(kElementDictionaryKey, &element_id) &&
         element_id->GetType() == Value::TYPE_STRING;
}

DictionaryValue* WebDriverCommand::GetElementIdAsDictionaryValue(
    const std::string& element_id) {
  DictionaryValue* dictionary = new DictionaryValue;
  dictionary->SetString(kElementDictionaryKey, element_id);
  return dictionary;
}

bool WebDriverCommand::Init(Response* const response) {
  // There should be at least 3 path segments to match "/session/$id".
  std::string session_id = GetPathVariable(2);
  if (session_id.length() == 0) {
    response->set_value(Value::CreateStringValue("No session ID specified"));
    response->set_status(kBadRequest);
    return false;
  }

  VLOG(1) << "Fetching session: " << session_id;
  session_ = SessionManager::GetInstance()->GetSession(session_id);
  if (session_ == NULL) {
    response->set_value(Value::CreateStringValue(
        "Session not found: " + session_id));
    response->set_status(kSessionNotFound);
    return false;
  }

  response->SetField("sessionId", Value::CreateStringValue(session_id));
  return true;
}

}  // namespace webdriver
