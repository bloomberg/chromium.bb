// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/identity/oauth2_manifest_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_mint_token_flow.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::ReturnRef;

namespace extensions {

namespace {

namespace errors = identity_constants;
namespace utils = extension_function_test_utils;

static const char kAccessToken[] = "auth_token";

class TestLoginUI : public LoginUIService::LoginUI {
 public:
  virtual void FocusUI() OVERRIDE {}
  virtual void CloseUI() OVERRIDE {}
};

class TestOAuth2MintTokenFlow : public OAuth2MintTokenFlow {
 public:
  enum ResultType {
    ISSUE_ADVICE_SUCCESS,
    MINT_TOKEN_SUCCESS,
    MINT_TOKEN_FAILURE
  };

  TestOAuth2MintTokenFlow(ResultType result,
                          OAuth2MintTokenFlow::Delegate* delegate)
    : OAuth2MintTokenFlow(NULL, delegate, OAuth2MintTokenFlow::Parameters()),
      result_(result),
      delegate_(delegate) {
  }

  virtual void Start() OVERRIDE {
    switch (result_) {
      case ISSUE_ADVICE_SUCCESS: {
        IssueAdviceInfo info;
        delegate_->OnIssueAdviceSuccess(info);
        break;
      }
      case MINT_TOKEN_SUCCESS: {
        delegate_->OnMintTokenSuccess(kAccessToken);
        break;
      }
      case MINT_TOKEN_FAILURE: {
        GoogleServiceAuthError error(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
        delegate_->OnMintTokenFailure(error);
        break;
      }
    }
  }

 private:
  ResultType result_;
  OAuth2MintTokenFlow::Delegate* delegate_;
};

}  // namespace

class MockGetAuthTokenFunction : public IdentityGetAuthTokenFunction {
 public:
  MockGetAuthTokenFunction() : install_ui_result_(false),
                               login_ui_shown_(false),
                               install_ui_shown_(false) {
  }

  void set_install_ui_result(bool result) {
    install_ui_result_ = result;
  }

  bool login_ui_shown() const {
    return login_ui_shown_;
  }

  bool install_ui_shown() const {
    return install_ui_shown_;
  }

  virtual void StartObservingLoginService() OVERRIDE {
    // Do nothing in tests.
  }
  virtual void StopObservingLoginService() OVERRIDE {
    // Do nothing in tests.
  }

  virtual void ShowLoginPopup() OVERRIDE {
    login_ui_shown_ = true;
    // Explicitly call OnLoginUIClosed.
    TestLoginUI login_ui;;
    OnLoginUIClosed(&login_ui);
  }

  virtual void ShowOAuthApprovalDialog(
      const IssueAdviceInfo& issue_advice) OVERRIDE {
    install_ui_shown_ = true;
    install_ui_->record_oauth2_grant_ = true;
    // Call InstallUIProceed or InstallUIAbort based on the flag.
    if (install_ui_result_)
      InstallUIProceed();
    else
      InstallUIAbort(true);
  }

  MOCK_CONST_METHOD0(HasLoginToken, bool ());
  MOCK_METHOD1(CreateMintTokenFlow,
               OAuth2MintTokenFlow* (OAuth2MintTokenFlow::Mode mode));
 private:
  ~MockGetAuthTokenFunction() {}
  bool install_ui_result_;
  bool login_ui_shown_;
  bool install_ui_shown_;
};

class GetAuthTokenFunctionTest : public ExtensionBrowserTest {
 protected:
  enum OAuth2Fields {
    NONE = 0,
    CLIENT_ID = 1,
    SCOPES = 2
  };

  virtual ~GetAuthTokenFunctionTest() {}

  // Helper to create an extension with specific OAuth2Info fields set.
  // |fields_to_set| should be computed by using fields of Oauth2Fields enum.
  const Extension* CreateExtension(int fields_to_set) {
    const Extension* ext = LoadExtension(
        test_data_dir_.AppendASCII("platform_apps/oauth2"));
    OAuth2Info& oauth2_info = const_cast<OAuth2Info&>(
        OAuth2Info::GetOAuth2Info(ext));
    if ((fields_to_set & CLIENT_ID) != 0)
      oauth2_info.client_id = "client1";
    if ((fields_to_set & SCOPES) != 0) {
      oauth2_info.scopes.push_back("scope1");
      oauth2_info.scopes.push_back("scope2");
    }
    return ext;
  }
};

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NoClientId) {
  scoped_refptr<MockGetAuthTokenFunction> func(new MockGetAuthTokenFunction());
  func->set_extension(CreateExtension(SCOPES));
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{}]", browser());
  EXPECT_EQ(std::string(errors::kInvalidClientId), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NoScopes) {
  scoped_refptr<MockGetAuthTokenFunction> func(new MockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID));
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{}]", browser());
  EXPECT_EQ(std::string(errors::kInvalidScopes), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveNotSignedIn) {
  scoped_refptr<MockGetAuthTokenFunction> func(new MockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken()).WillOnce(Return(false));
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{}]", browser());
  EXPECT_EQ(std::string(errors::kUserNotSignedIn), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveMintFailure) {
  scoped_refptr<MockGetAuthTokenFunction> func(new MockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken())
      .WillOnce(Return(true))
      .WillOnce(Return(true));
  TestOAuth2MintTokenFlow* flow = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::MINT_TOKEN_FAILURE, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_)).WillOnce(Return(flow));
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{}]", browser());
  EXPECT_TRUE(StartsWithASCII(error, errors::kAuthFailure, false));
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       NonInteractiveSuccess) {
  scoped_refptr<MockGetAuthTokenFunction> func(new MockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken())
      .WillOnce(Return(true))
      .WillOnce(Return(true));
  TestOAuth2MintTokenFlow* flow = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_)).WillOnce(Return(flow));
  scoped_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      func.get(), "[{}]", browser()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginCanceled) {
  scoped_refptr<MockGetAuthTokenFunction> func(new MockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken()).WillRepeatedly(Return(false));
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_EQ(std::string(errors::kUserNotSignedIn), error);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginSuccessMintFailure) {
  scoped_refptr<MockGetAuthTokenFunction> func(new MockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken())
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  TestOAuth2MintTokenFlow* flow = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::MINT_TOKEN_FAILURE, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_)).WillOnce(Return(flow));
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_TRUE(StartsWithASCII(error, errors::kAuthFailure, false));
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginSuccessMintSuccess) {
  scoped_refptr<MockGetAuthTokenFunction> func(new MockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken())
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  TestOAuth2MintTokenFlow* flow = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_)).WillOnce(Return(flow));
  scoped_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      func.get(), "[{\"interactive\": true}]", browser()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginSuccessApprovalAborted) {
  scoped_refptr<MockGetAuthTokenFunction> func(new MockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken())
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  TestOAuth2MintTokenFlow* flow = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_)).WillOnce(Return(flow));
  func->set_install_ui_result(false);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_EQ(std::string(errors::kUserRejected), error);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_TRUE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginSuccessApprovalDoneMintFailure) {
  scoped_refptr<MockGetAuthTokenFunction> func(new MockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken())
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .WillOnce(Return(true));
  TestOAuth2MintTokenFlow* flow1 = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS, func.get());
  TestOAuth2MintTokenFlow* flow2 = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::MINT_TOKEN_FAILURE, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_))
      .WillOnce(Return(flow1))
      .WillOnce(Return(flow2));

  func->set_install_ui_result(true);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_TRUE(StartsWithASCII(error, errors::kAuthFailure, false));
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_TRUE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveLoginSuccessApprovalDoneMintSuccess) {
  scoped_refptr<MockGetAuthTokenFunction> func(new MockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken())
      .WillOnce(Return(false))
      .WillOnce(Return(true))
      .WillOnce(Return(true));
  TestOAuth2MintTokenFlow* flow1 = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS, func.get());
  TestOAuth2MintTokenFlow* flow2 = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_))
      .WillOnce(Return(flow1))
      .WillOnce(Return(flow2));

  func->set_install_ui_result(true);
  scoped_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      func.get(), "[{\"interactive\": true}]", browser()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_TRUE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveApprovalAborted) {
  scoped_refptr<MockGetAuthTokenFunction> func(new MockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken())
      .WillOnce(Return(true))
      .WillOnce(Return(true));
  TestOAuth2MintTokenFlow* flow = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_)).WillOnce(Return(flow));
  func->set_install_ui_result(false);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_EQ(std::string(errors::kUserRejected), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_TRUE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(GetAuthTokenFunctionTest,
                       InteractiveApprovalDoneMintSuccess) {
  scoped_refptr<MockGetAuthTokenFunction> func(new MockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken())
      .WillOnce(Return(true))
      .WillOnce(Return(true))
      .WillOnce(Return(true));
  TestOAuth2MintTokenFlow* flow1 = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS, func.get());
  TestOAuth2MintTokenFlow* flow2 = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_))
      .WillOnce(Return(flow1))
      .WillOnce(Return(flow2));

  func->set_install_ui_result(true);
  scoped_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      func.get(), "[{\"interactive\": true}]", browser()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_TRUE(func->install_ui_shown());
}

class LaunchWebAuthFlowFunctionTest : public ExtensionBrowserTest {
 protected:
  void RunAndCheckBounds(
      const std::string& extra_params,
      int expected_x,
      int expected_y,
      int expected_width,
      int expected_height) {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_BROWSER_WINDOW_READY,
        content::NotificationService::AllSources());

   scoped_refptr<IdentityLaunchWebAuthFlowFunction> function(
      new IdentityLaunchWebAuthFlowFunction());
    scoped_refptr<Extension> empty_extension(
        utils::CreateEmptyExtension());
    function->set_extension(empty_extension.get());
    std::string args = base::StringPrintf(
        "[{\"interactive\": true, \"url\": \"data:text/html,auth\"%s%s}]",
        extra_params.length() ? "," : "",
        extra_params.c_str());
    scoped_ptr<base::ListValue> parsed_args(utils::ParseList(args));
    EXPECT_TRUE(parsed_args.get()) <<
          "Could not parse extension function arguments: " << args;
    function->SetArgs(parsed_args.get());
    function->set_profile(browser()->profile());

    // We don't use util::RunFunction because that waits until the function
    // responds, but we want to check the size of the created browser as soon as
    // it's created, even though there's no actual redirect to an auth URL.
    function->Run();

    observer.Wait();

    Browser* web_auth_flow_browser =
        content::Source<Browser>(observer.source()).ptr();
    EXPECT_EQ(expected_x, web_auth_flow_browser->override_bounds().x());
    EXPECT_EQ(expected_y, web_auth_flow_browser->override_bounds().y());
    EXPECT_EQ(expected_width, web_auth_flow_browser->override_bounds().width());
    EXPECT_EQ(
        expected_height, web_auth_flow_browser->override_bounds().height());

    web_auth_flow_browser->window()->Close();
  }
};

IN_PROC_BROWSER_TEST_F(LaunchWebAuthFlowFunctionTest, Bounds) {
  RunAndCheckBounds("", 0, 0, 0, 0);
  RunAndCheckBounds("\"width\": 100, \"height\": 200", 0, 0, 100, 200);
  RunAndCheckBounds("\"left\": 100, \"top\": 200", 100, 200, 0, 0);
  RunAndCheckBounds(
      "\"left\": 100, \"top\": 200, \"width\": 300, \"height\": 400",
      100, 200, 300, 400);
}

}  // namespace extensions
