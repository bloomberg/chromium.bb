// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Return;

namespace chromeos {

namespace {

const char kTestUser1[] = "test-user1@gmail.com";
const char kTestUser2[] = "test-user2@gmail.com";

}  // anonymous namespace

class LoginUITest : public chromeos::LoginManagerTest {
 public:
  LoginUITest() : LoginManagerTest(false) {}
  virtual ~LoginUITest() {}
};

IN_PROC_BROWSER_TEST_F(LoginUITest, PRE_LoginUIVisible) {
  RegisterUser(kTestUser1);
  RegisterUser(kTestUser2);
  StartupUtils::MarkOobeCompleted();
}

// Verifies basic login UI properties.
IN_PROC_BROWSER_TEST_F(LoginUITest, LoginUIVisible) {
  JSExpect("!!document.querySelector('#account-picker')");
  JSExpect("!!document.querySelector('#pod-row')");
  JSExpect(
      "document.querySelectorAll('.pod:not(#user-pod-template)').length == 2");

  JSExpect("document.querySelectorAll('.pod:not(#user-pod-template)')[0]"
           ".user.emailAddress == '" + std::string(kTestUser1) + "'");
  JSExpect("document.querySelectorAll('.pod:not(#user-pod-template)')[1]"
           ".user.emailAddress == '" + std::string(kTestUser2) + "'");
}

}  // namespace chromeos
