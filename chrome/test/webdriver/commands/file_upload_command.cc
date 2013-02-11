// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/commands/file_upload_command.h"

#include "base/file_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/test/webdriver/commands/response.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_session.h"
#include "chrome/test/webdriver/webdriver_util.h"

namespace webdriver {

FileUploadCommand::FileUploadCommand(
    const std::vector<std::string>& path_segments,
    DictionaryValue* parameters)
    : WebDriverCommand(path_segments, parameters) {
}

FileUploadCommand::~FileUploadCommand() {
}

bool FileUploadCommand::DoesPost() {
  return true;
}

void FileUploadCommand::ExecutePost(Response* const response) {
  std::string base64_zip_data;
  if (!GetStringParameter("file", &base64_zip_data)) {
    response->SetError(new Error(kUnknownError, "Missing or invalid 'file'"));
    return;
  }
  std::string zip_data;
  if (!Base64Decode(base64_zip_data, &zip_data)) {
    response->SetError(new Error(kUnknownError, "Unable to decode 'file'"));
    return;
  }

  base::FilePath upload_dir;
  if (!file_util::CreateTemporaryDirInDir(
          session_->temp_dir(), FILE_PATH_LITERAL("upload"), &upload_dir)) {
    response->SetError(new Error(kUnknownError, "Failed to create temp dir"));
    return;
  }
  std::string error_msg;
  base::FilePath upload;
  if (!UnzipSoleFile(upload_dir, zip_data, &upload, &error_msg)) {
    response->SetError(new Error(kUnknownError, error_msg));
    return;
  }

#if defined(OS_WIN)
  response->SetValue(new base::StringValue(WideToUTF8(upload.value())));
#else
  response->SetValue(new base::StringValue(upload.value()));
#endif
}

}  // namespace webdriver
