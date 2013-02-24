// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MINI_INSTALLER_TEST_INSTALLER_TEST_UTIL_H_
#define CHROME_TEST_MINI_INSTALLER_TEST_INSTALLER_TEST_UTIL_H_

#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/installer/util/installation_validator.h"
#include "chrome/test/mini_installer_test/switch_builder.h"

namespace installer_test {

struct InstalledProduct {
  std::string version;
  installer::InstallationValidator::InstallationType type;
  bool system;
};

// Utility functions used to test Chrome installation.

// Deletes the Chrome installation directory which is found at different
// locations depending on the |system_level| and |type|.
// Returns true if successful, otherwise false.
bool DeleteInstallDirectory(
    bool system_level,
    installer::InstallationValidator::InstallationType type);

// Deletes the Chrome Windows registry entry.
// Returns true if successful, otherwise false.
bool DeleteRegistryKey(
    bool system_level,
    installer::InstallationValidator::InstallationType type);

// Locates the Chrome installation directory based on the
// provided |system_level|. Returns true if successful, otherwise false.
// Returns true if successful, otherwise false.
bool GetChromeInstallDirectory(bool system_level, base::FilePath* path);

// Gets the installation directory of either Chrome or Chrome Frame
// as specified by the |system_level| and |type|.
// Returns true if successful, otherwise false.
bool GetInstallDirectory(bool system_level,
                         BrowserDistribution::Type type, base::FilePath* path);

// Returns the version of the specified |product|.
std::string GetVersion(
    installer::InstallationValidator::InstallationType product);

// Gets a list of installed products.
// Returns true if there are installed products.
bool GetInstalledProducts(std::vector<InstalledProduct>* products);

bool Install(const base::FilePath& installer);
bool Install(const base::FilePath& installer, const SwitchBuilder& switches);
bool LaunchChrome(bool close_after_launch, bool system_level);
bool LaunchIE(const std::string& url);

// Uninstall all Chrome or Chrome Frame installations.
bool UninstallAll();

// Uninstall the product specified by |system_level| and |type|.
bool Uninstall(
    bool system_level,
    installer::InstallationValidator::InstallationType type);

// Uninstall the product specified by |system_level| and |product|.
bool Uninstall(
    bool system_level,
    BrowserDistribution::Type product);

bool ValidateInstall(
     bool system_level,
     installer::InstallationValidator::InstallationType expected,
     const std::string& version);

// Run and wait for command to finish.
// Returns true if successful, otherwise false.
bool RunAndWaitForCommandToFinish(CommandLine command);

}  // namespace

#endif  // CHROME_TEST_MINI_INSTALLER_TEST_INSTALLER_TEST_UTIL_H_

