// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_INSTALLER_WIN_APP_INSTALLER_UTIL_H_
#define CHROME_APP_INSTALLER_WIN_APP_INSTALLER_UTIL_H_

#include <map>
#include <string>

#include "base/files/file_path.h"

namespace app_installer {

extern const char kInstallChromeApp[];

enum ExitCode {
  SUCCESS = 0,
  COULD_NOT_GET_FILE_PATH,
  COULD_NOT_READ_TAG,
  COULD_NOT_PARSE_TAG,
  INVALID_APP_ID,
  COULD_NOT_FETCH_INLINE_INSTALL_DATA,
  COULD_NOT_PARSE_INLINE_INSTALL_DATA,
  EULA_CANCELLED,
  COULD_NOT_FIND_CHROME,
  COULD_NOT_GET_TMP_FILE_PATH,
  FAILED_TO_DOWNLOAD_CHROME_SETUP,
  FAILED_TO_LAUNCH_CHROME_SETUP,
};

// Gets the tag attached to a file by dl.google.com. This uses the same format
// as Omaha. Returns the empty string on failure.
std::string GetTag(const base::FilePath& file_name_path);

// Parses |tag| as key-value pairs and overwrites |parsed_pairs| with
// the result. |tag| should be a '&'-delimited list of '='-separated
// key-value pairs, e.g. "key1=value1&key2=value2".
// Returns true if the data could be parsed.
bool ParseTag(const std::string& tag,
              std::map<std::string, std::string>* parsed_pairs);

bool IsValidAppId(const std::string& app_id);

// Uses WinHTTP to make a GET request. |server_port| can be zero to use the
// default port (80 or 443).
bool FetchUrl(const base::string16& user_agent,
              const base::string16& server_name,
              uint16_t server_port,
              const base::string16& object_name,
              std::vector<uint8_t>* response_data);

// Shows UI to download and install Chrome. Returns a failure code, or SUCCESS
// if the installation completed successfully.
ExitCode GetChrome(bool is_canary, const std::string& inline_install_json);

}  // namespace app_installer

#endif  // CHROME_APP_INSTALLER_WIN_APP_INSTALLER_UTIL_H_
