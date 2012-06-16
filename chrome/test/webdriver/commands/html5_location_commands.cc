// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/html5_location_commands.h"

#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_session.h"

namespace webdriver {

HTML5LocationCommand::HTML5LocationCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

HTML5LocationCommand::~HTML5LocationCommand() {}

bool HTML5LocationCommand::DoesGet() {
  return true;
}

bool HTML5LocationCommand::DoesPost() {
  return true;
}

void HTML5LocationCommand::ExecuteGet(Response* const response) {
  scoped_ptr<base::DictionaryValue> geolocation;
  Error* error = session_->GetGeolocation(&geolocation);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(geolocation.release());
}

void HTML5LocationCommand::ExecutePost(Response* const response) {
  base::DictionaryValue* geolocation;
  if (!GetDictionaryParameter("location", &geolocation)) {
    response->SetError(new Error(
        kBadRequest, "Missing or invalid 'location'"));
    return;
  }
  Error* error = session_->OverrideGeolocation(geolocation);
  if (error)
    response->SetError(error);
}

}  // namespace webdriver
