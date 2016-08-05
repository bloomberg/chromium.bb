// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TOOLS_DISABLE_OUTDATED_BUILD_DETECTOR_GOOGLE_UPDATE_INTEGRATION_H_
#define CHROME_TOOLS_DISABLE_OUTDATED_BUILD_DETECTOR_GOOGLE_UPDATE_INTEGRATION_H_

#include "base/strings/string16.h"
#include "chrome/tools/disable_outdated_build_detector/constants.h"

namespace base {
namespace win {
class RegKey;
}
}

// A structure containing the values that comprise the Google Update installer
// result API.
struct ResultInfo {
  InstallerResult installer_result;
  ExitCode installer_error;
  uint32_t installer_extra_code1;
};

// Opens the Google Update ClientState key in the registry for the app
// identified by |app_guid| at |system_level|. Returns ERROR_SUCCESS or another
// Windows error value.
uint32_t OpenClientStateKey(bool system_level,
                            const wchar_t* app_guid,
                            base::win::RegKey* key);

// Writes the data in |result_info| into Chrome's ClientState key.
void WriteResultInfo(bool system_level, const ResultInfo& result_info);

// Returns |true| if |brand| represents an organic brand code.
bool IsOrganic(const base::string16& brand);

#endif  // CHROME_TOOLS_DISABLE_OUTDATED_BUILD_DETECTOR_GOOGLE_UPDATE_INTEGRATION_H_
