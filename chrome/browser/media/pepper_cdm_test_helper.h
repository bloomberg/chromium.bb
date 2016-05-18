// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_PEPPER_CDM_TEST_HELPER_H_
#define CHROME_BROWSER_MEDIA_PEPPER_CDM_TEST_HELPER_H_

#include <string>

#include "base/files/file_path.h"

namespace base {
class CommandLine;
}

// Display name for Clear Key CDM.
extern const char kClearKeyCdmDisplayName[];

// Platform-specific filename relative to kClearKeyCdmBaseDirectory.
extern const char kClearKeyCdmAdapterFileName[];

// Pepper type for Clear Key CDM.
extern const char kClearKeyCdmPepperMimeType[];

// Builds the string to pass to kRegisterPepperPlugins for a single
// CDM using the provided parameters and a dummy version.
// Multiple results may be passed to kRegisterPepperPlugins, separated by ",".
// The CDM adapter should be located in DIR_MODULE.
base::FilePath::StringType BuildPepperCdmRegistration(
    const std::string& adapter_file_name,
    const std::string& display_name,
    const std::string& mime_type,
    bool expect_adapter_exists = true);

// Registers pepper CDM in |command_line|.
void RegisterPepperCdm(base::CommandLine* command_line,
                       const std::string& adapter_file_name,
                       const std::string& display_name,
                       const std::string& mime_type,
                       bool expect_adapter_exists = true);

#endif  // CHROME_BROWSER_MEDIA_PEPPER_CDM_TEST_HELPER_H_
