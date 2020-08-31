// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants for Chrome OS test user accounts.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_TEST_USERS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_TEST_USERS_H_

namespace chromeos {

namespace saml_test_users {

// Note that the "corp.example.com" and the "example.test" domains are
// important, since they're hardcoded in embedded_setup_chromeos.html.
extern const char kFirstUserCorpExampleComEmail[];
extern const char kSecondUserCorpExampleComEmail[];
extern const char kThirdUserCorpExampleComEmail[];
extern const char kFourthUserCorpExampleTestEmail[];
extern const char kFifthUserExampleTestEmail[];

}  // namespace saml_test_users

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_TEST_USERS_H_
