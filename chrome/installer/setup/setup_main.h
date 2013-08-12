// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This header exists as a starting point for extracting some of the
// logic out of setup_main.cc.

#ifndef CHROME_INSTALLER_SETUP_SETUP_MAIN_H_
#define CHROME_INSTALLER_SETUP_SETUP_MAIN_H_

#include "chrome/installer/util/util_constants.h"

class CommandLine;

namespace installer {

class InstallationState;
class InstallerState;
class MasterPreferences;

// Helper function that performs the installation of a set of products.
installer::InstallStatus InstallProductsHelper(
    const installer::InstallationState& original_state,
    const CommandLine& cmd_line,
    const installer::MasterPreferences& prefs,
    const installer::InstallerState& installer_state,
    installer::ArchiveType* archive_type,
    bool* delegated_to_existing);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_SETUP_MAIN_H_
