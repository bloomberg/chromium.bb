// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_LOGIN_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_LOGIN_UTILS_H_

#include <string>
#include <vector>
#include "chrome/browser/chromeos/login/login_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockLoginUtils : public chromeos::LoginUtils {
 public:
  virtual ~MockLoginUtils() {}
  MOCK_METHOD2(CompleteLogin, void(const std::string& username,
                                   std::vector<std::string> cookies));
  MOCK_METHOD1(CreateAuthenticator,
               Authenticator*(LoginStatusConsumer* consumer));
};

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_LOGIN_UTILS_H_
