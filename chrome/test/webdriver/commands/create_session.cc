// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/create_session.h"

#include <string>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/scoped_temp_dir.h"
#include "base/values.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/webdriver_capabilities_parser.h"
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
  ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDir()) {
    response->SetError(new Error(
        kUnknownError, "Unable to create temp directory for unpacking"));
    return;
  }
  Capabilities caps;
  CapabilitiesParser parser(dict, temp_dir.path(), &caps);
  Error* error = parser.Parse();
  if (error) {
    response->SetError(error);
    return;
  }

  // Since logging is shared among sessions, if any session requests verbose
  // logging, verbose logging will be enabled for all sessions. It is not
  // possible to turn it off.
  if (caps.verbose)
    logging::SetMinLogLevel(logging::LOG_INFO);

  Session::Options session_options;
  session_options.load_async = caps.load_async;
  session_options.use_native_events = caps.native_events;

  Automation::BrowserOptions browser_options;
  browser_options.command = caps.command;
  browser_options.channel_id = caps.channel;
  browser_options.detach_process = caps.detach;
  browser_options.user_data_dir = caps.profile;

  // Session manages its own liftime, so do not call delete.
  Session* session = new Session(session_options);
  error = session->Init(browser_options);
  if (error) {
    response->SetError(error);
    return;
  }

  // Install extensions.
  for (size_t i = 0; i < caps.extensions.size(); ++i) {
    Error* error = session->InstallExtensionDeprecated(caps.extensions[i]);
    if (error) {
      response->SetError(error);
      return;
    }
  }

  LOG(INFO) << "Created session " << session->id();
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
