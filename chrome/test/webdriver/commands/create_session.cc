// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/create_session.h"

#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/scoped_temp_dir.h"
#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_session.h"
#include "chrome/test/webdriver/webdriver_session_manager.h"

namespace webdriver {

CreateSession::CreateSession(const std::vector<std::string>& path_segments,
                             const DictionaryValue* const parameters)
    : Command(path_segments, parameters) {}

CreateSession::~CreateSession() {}

bool CreateSession::DoesPost() { return true; }

void CreateSession::ExecutePost(Response* const response) {
  DictionaryValue* dict;
  if (!GetDictionaryParameter("desiredCapabilities", &dict)) {
    response->SetError(new Error(
        kBadRequest, "Missing or invalid 'desiredCapabilities'"));
    return;
  }

  // Session manages its own liftime, so do not call delete.
  Session* session = new Session();
  Error* error = session->Init(dict);
  if (error) {
    response->SetError(error);
    return;
  }

  // Redirect to a relative URI. Although prohibited by the HTTP standard,
  // this is what the IEDriver does. Finding the actual IP address is
  // difficult, and returning the hostname causes perf problems with the python
  // bindings on Windows.
  std::ostringstream stream;
  stream << SessionManager::GetInstance()->url_base() << "/session/"
         << session->id();
  response->SetStatus(kSeeOther);
  response->SetValue(Value::CreateStringValue(stream.str()));
}

}  // namespace webdriver
