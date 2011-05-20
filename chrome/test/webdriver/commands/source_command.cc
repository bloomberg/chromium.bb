// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/source_command.h"

#include <string>

#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/session.h"
#include "chrome/test/webdriver/webdriver_error.h"

namespace webdriver {

// Private atom to find source code of the page.
const char* const kSource =
    "return new XMLSerializer().serializeToString(document);";

SourceCommand::SourceCommand(const std::vector<std::string>& path_segments,
                             const DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

SourceCommand::~SourceCommand() {}

bool SourceCommand::DoesGet() {
  return true;
}

void SourceCommand::ExecuteGet(Response* const response) {
  ListValue args;
  Value* result = NULL;
  Error* error = session_->ExecuteScript(kSource, &args, &result);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(result);
}

}  // namespace webdriver
