// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/values.h"
#include "chrome/browser/chromeos/webui/login/login_ui.h"
#include "chrome/browser/chromeos/webui/login/login_ui_unittest.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrEq;

// Listing of untested functions:
//
// LoginUI::*
// LoginUIHTML::*
//
// The objects LoginUI and LoginUIHTMLSource do not have any testing,
// since there are rather trivial/boilerplatey. They are used for hooking the
// LoginUI code into the greater system and depend extensively on calling
// external functions. The external functions should be unit tested, but the
// these objects are better tested in a functional manner.
//
//
// LoginUIHandler::RegisterMessage
//
// There is currently no test for LoginUIHandler::RegisterMessage, b/c the
// function WebUI::RegisterMesssageCallback is not declared virtual. This means
// that it cannot be mocked easily and it is non-trivial to resolve, since just
// making the function virtual causes other problems. Either this class or that
// class needs to be refactored to deal with this before one can write this
// test. Realistically there isn't much to fail in this function, so testing
// should not be needed in this class.
//
//
// LoginUIHandler::HandleLaunchIncognito
// LoginUIHandler::OnLoginFailure
// LoginUIHandler::OnLoginSuccess
// LoginUIHandler::OnOffTheRecordLoginSuccess
//
// There no tests for these functions since all of them are pretty straight
// forward assuming that the called functions perform as expected and it is
// non-trivial to mock the Browser class.
namespace chromeos {

// LoginUIHandler::Attach
TEST(LoginUIHandlerTest, Attach) {
  // Don't care about the expected in this test
  LoginUIHandlerHarness handler_harness("", "");
  MockWebUI web_ui;

  EXPECT_EQ(&handler_harness, handler_harness.Attach(&web_ui));
  EXPECT_EQ(&web_ui, handler_harness.GetWebUI());
}

// Helper for LoginUIHandler::HandleAuthenticateUser
void RunHandleAuthenticateUserTest(const std::string& expected_username,
                                   const std::string& expected_password,
                                   const std::string& supplied_username,
                                   const std::string& supplied_password) {
  ListValue arg_list;
  StringValue* username_value = new StringValue(supplied_username);
  StringValue* password_value = new StringValue(supplied_password);
  TestingProfile mock_profile;
  LoginUIHandlerHarness handler(expected_username,
                                          expected_password);

  EXPECT_EQ(expected_username, handler.GetMockFacade()->GetUsername());
  EXPECT_EQ(expected_password, handler.GetMockFacade()->GetPassword());

  arg_list.Append(username_value);
  arg_list.Append(password_value);
  EXPECT_CALL(*(handler.GetMockProfileOperations()),
              GetDefaultProfile())
              .Times(1)
              .WillRepeatedly(Return(&mock_profile));

  EXPECT_CALL(*(handler.GetMockFacade()),
              AuthenticateToLogin(&mock_profile,
                                  StrEq(supplied_username),
                                  StrEq(supplied_password),
                                  StrEq(std::string()),
                                  StrEq(std::string())))
              .Times(1);

  handler.HandleAuthenticateUser(&arg_list);
  // This code does not simulate the callback that occurs in the
  // AuthenticatorFacade, since that would be a) a functional test and b)
  // require a working mock of Browser.
}

// LoginUIHandler::HandleAuthenticateUser on success
TEST(LoginUIHandlerTest, HandleAuthenticateUserSuccess) {
  RunHandleAuthenticateUserTest("chronos",
                                "chronos",
                                "chronos",
                                "chronos");

  RunHandleAuthenticateUserTest("bob",
                                "mumistheword",
                                "bob",
                                "mumistheword");
}

// LoginUIHandler::HandleAuthenticateUser on failure
TEST(LoginUIHandlerTest, HandleAuthenticateUserFailure) {
  RunHandleAuthenticateUserTest("chronos",
                                "chronos",
                                std::string(),
                                "chronos");

  RunHandleAuthenticateUserTest("chronos",
                                "chronos",
                                std::string(),
                                std::string());

  RunHandleAuthenticateUserTest("chronos",
                                "chronos",
                                "chronos",
                                std::string());

  RunHandleAuthenticateUserTest("chronos",
                                "chronos",
                                "bob",
                                "mumistheword");

  RunHandleAuthenticateUserTest("bob",
                                "mumistheword",
                                "bob",
                                std::string());

  RunHandleAuthenticateUserTest("bob",
                                "mumistheword",
                                std::string(),
                                std::string());

  RunHandleAuthenticateUserTest("bob",
                                "mumistheword",
                                std::string(),
                                "mumistheword");

  RunHandleAuthenticateUserTest("bob",
                                "mumistheword",
                                "chronos",
                                "chronos");
}

}  // namespace chromeos
