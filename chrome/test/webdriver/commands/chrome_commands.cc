// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/chrome_commands.h"

#include <string>
#include <vector>

#include "base/file_path.h"
#include "chrome/test/automation/value_conversion_util.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_session.h"

namespace webdriver {

ExtensionsCommand::ExtensionsCommand(
    const std::vector<std::string>& path_segments,
    const base::DictionaryValue* const parameters)
    : WebDriverCommand(path_segments, parameters) {}

ExtensionsCommand::~ExtensionsCommand() {}

bool ExtensionsCommand::DoesGet() {
  return true;
}

bool ExtensionsCommand::DoesPost() {
  return true;
}

void ExtensionsCommand::ExecuteGet(Response* const response) {
  std::vector<std::string> extension_ids;
  Error* error = session_->GetInstalledExtensions(&extension_ids);
  if (error) {
    response->SetError(error);
    return;
  }
  base::ListValue* extensions = new base::ListValue();
  for (size_t i = 0; i < extension_ids.size(); ++i)
    extensions->Append(CreateValueFrom(extension_ids[i]));
  response->SetValue(extensions);
}

void ExtensionsCommand::ExecutePost(Response* const response) {
  FilePath::StringType path_string;
  if (!GetStringParameter("path", &path_string)) {
    response->SetError(new Error(kUnknownError, "'path' missing or invalid"));
    return;
  }

  std::string extension_id;
  Error* error = session_->InstallExtension(
      FilePath(path_string), &extension_id);
  if (error) {
    response->SetError(error);
    return;
  }
  response->SetValue(CreateValueFrom(extension_id));
}

}  // namespace webdriver
