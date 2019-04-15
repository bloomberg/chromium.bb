// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ACTIVE_DIRECTORY_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ACTIVE_DIRECTORY_TEST_HELPER_H_

#include <string>

#include "chromeos/tpm/install_attributes.h"

namespace chromeos {

namespace active_directory_test_helper {

// Locks the device to Active Directory mode.
InstallAttributes::LockResult LockDevice(const std::string& domain);

// Sets stub path overrides.
void OverridePaths();

}  // namespace active_directory_test_helper

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ACTIVE_DIRECTORY_TEST_HELPER_H_
