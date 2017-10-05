// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_LIBRARY_CDM_TEST_HELPER_H_
#define CHROME_BROWSER_MEDIA_LIBRARY_CDM_TEST_HELPER_H_

#include <string>

#include "base/files/file_path.h"

namespace base {
class CommandLine;
}

// Registers external clear key CDM in |command_line|.
void RegisterExternalClearKey(base::CommandLine* command_line,
                              bool expect_cdm_exists = true);

bool IsLibraryCdmRegistered(const std::string& cdm_guid);

// TODO(crbug.com/403462): Remove the following after pepper CDM is deprecated.

// Returns the path a pepper CDM adapter.
base::FilePath GetPepperCdmPath(const std::string& adapter_base_dir,
                                const std::string& adapter_file_name);

// Builds the string to pass to kRegisterPepperPlugins for a single
// CDM using the provided parameters and a dummy version.
// Multiple results may be passed to kRegisterPepperPlugins, separated by ",".
// The CDM adapter should be located in DIR_MODULE.
base::FilePath::StringType BuildPepperCdmRegistration(
    const std::string& adapter_base_dir,
    const std::string& adapter_file_name,
    const std::string& display_name,
    const std::string& mime_type,
    bool expect_adapter_exists = true);

// Registers pepper CDM in |command_line|.
void RegisterPepperCdm(base::CommandLine* command_line,
                       const std::string& adapter_base_dir,
                       const std::string& adapter_file_name,
                       const std::string& display_name,
                       const std::string& mime_type,
                       bool expect_adapter_exists = true);

// Returns whether a pepper CDM with |mime_type| is registered.
bool IsPepperCdmRegistered(const std::string& mime_type);

#endif  // CHROME_BROWSER_MEDIA_LIBRARY_CDM_TEST_HELPER_H_
