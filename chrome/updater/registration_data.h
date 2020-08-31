// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_REGISTRATION_DATA_H_
#define CHROME_UPDATER_REGISTRATION_DATA_H_

#include <string>

#include "base/files/file_path.h"
#include "base/version.h"
#include "chrome/updater/constants.h"

namespace updater {

struct RegistrationRequest {
  RegistrationRequest();
  RegistrationRequest(const RegistrationRequest&);
  RegistrationRequest& operator=(const RegistrationRequest& other) = default;
  ~RegistrationRequest();
  // Application ID of the app.
  std::string app_id;

  // The brand code, a four character code attributing the app’s
  // presence to a marketing campaign or similar effort. May be the empty
  // string.
  std::string brand_code;

  // The tag value (e.g. from a tagged metainstaller). May be the empty string.
  // This typically indicates channel, though it can carry additional data as
  // well.
  std::string tag;

  // The version of the app already installed. 0.0.0.0 if the app is not
  // already installed.
  base::Version version = base::Version(kNullVersion);

  // A file path. A file exists at this path if and only if the app is
  // still installed. This is used (on Mac, for example) to detect
  // whether an app has been uninstalled via deletion. May be the empty
  // string; if so, the app is assumed to be installed unconditionally.
  base::FilePath existence_checker_path;
};

struct RegistrationResponse {
  RegistrationResponse() = default;
  explicit RegistrationResponse(int status_code) : status_code(status_code) {}
  ~RegistrationResponse() = default;

  // Status code of the registration. 0 = success. All others = failure.
  int status_code = 0;
};

}  // namespace updater

#endif  // CHROME_UPDATER_REGISTRATION_DATA_H_
