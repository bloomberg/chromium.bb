// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ACTIVE_DIRECTORY_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ACTIVE_DIRECTORY_TEST_HELPER_H_

#include <string>

namespace chromeos {

namespace active_directory_test_helper {

// Starts AuthPolicyService, joins the Active Directory domain using
// |user_principal_name| for authentication (user@example.com), locks the device
// to Active Directory mode and fetches device policy.
void PrepareLogin(const std::string& user_principal_name);

// Sets stub path overrides.
void OverridePaths();

}  // namespace active_directory_test_helper

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ACTIVE_DIRECTORY_TEST_HELPER_H_
