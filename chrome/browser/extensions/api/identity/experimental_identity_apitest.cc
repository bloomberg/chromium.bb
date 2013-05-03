// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/identity/experimental_identity_api.h"
#include "chrome/browser/extensions/api/identity/identity_api.h"
#include "chrome/browser/extensions/api/identity/web_auth_flow.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/identity/oauth2_manifest_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/id_util.h"
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
static const char kExtensionId[] = "ext_id";

// This helps us be able to wait until an AsyncExtensionFunction calls
// SendResponse.
class SendResponseDelegate :
    public UIThreadExtensionFunction::DelegateForTests {
 public:
  SendResponseDelegate() : should_post_quit_(false) {}

  virtual ~SendResponseDelegate() {}

  void set_should_post_quit(bool should_quit) {
    should_post_quit_ = should_quit;
  }

  bool HasResponse() { return response_.get() != NULL; }

  bool GetResponse() {
    EXPECT_TRUE(HasResponse());
    return *response_.get();
  }

  virtual void OnSendResponse(UIThreadExtensionFunction* function,
                              bool success,
                              bool bad_message) OVERRIDE {
    ASSERT_FALSE(bad_message);
    ASSERT_FALSE(HasResponse());
    response_.reset(new bool);
    *response_ = success;
    if (should_post_quit_) {
      MessageLoopForUI::current()->Quit();
    }
  }

 private:
  scoped_ptr<bool> response_;
  bool should_post_quit_;
};

class AsyncExtensionBrowserTest : public ExtensionBrowserTest {
 protected:
  // Asynchronous function runner allows tests to manipulate the browser window
  // after the call happens.
  void RunFunctionAsync(UIThreadExtensionFunction* function,
                        const std::string& args) {
    response_delegate_.reset(new SendResponseDelegate);
    function->set_test_delegate(response_delegate_.get());
    scoped_ptr<base::ListValue> parsed_args(utils::ParseList(args));
    EXPECT_TRUE(parsed_args.get())
        << "Could not parse extension function arguments: " << args;
    function->SetArgs(parsed_args.get());

    if (!function->GetExtension()) {
      scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());
      function->set_extension(empty_extension.get());
    }

    function->set_profile(browser()->profile());
    function->set_has_callback(true);
    function->Run();
  }

  std::string WaitForError(UIThreadExtensionFunction* function) {
    RunMessageLoopUntilResponse();
    EXPECT_FALSE(function->GetResultList()) << "Did not expect a result";
    return function->GetError();
  }

  base::Value* WaitForSingleResult(UIThreadExtensionFunction* function) {
    RunMessageLoopUntilResponse();
    EXPECT_TRUE(function->GetError().empty())
        << "Unexpected error: " << function->GetError();
    const base::Value* single_result = NULL;
    if (function->GetResultList() != NULL &&
        function->GetResultList()->Get(0, &single_result)) {
      return single_result->DeepCopy();
    }
    return NULL;
  }

 private:
  void RunMessageLoopUntilResponse() {
    // If the RunImpl of |function| didn't already call SendResponse, run the
    // message loop until they do.
    if (!response_delegate_->HasResponse()) {
      response_delegate_->set_should_post_quit(true);
      content::RunMessageLoop();
    }
    EXPECT_TRUE(response_delegate_->HasResponse());
  }

  scoped_ptr<SendResponseDelegate> response_delegate_;
};

class TestOAuth2MintTokenFlow : public OAuth2MintTokenFlow {
 public:
  enum ResultType {
    ISSUE_ADVICE_SUCCESS,
    MINT_TOKEN_SUCCESS,
    MINT_TOKEN_FAILURE,
    MINT_TOKEN_BAD_CREDENTIALS
  };

  TestOAuth2MintTokenFlow(ResultType result,
                          OAuth2MintTokenFlow::Delegate* delegate)
      : OAuth2MintTokenFlow(NULL, delegate, OAuth2MintTokenFlow::Parameters()),
        result_(result),
        delegate_(delegate) {}

  virtual void Start() OVERRIDE {
    switch (result_) {
      case ISSUE_ADVICE_SUCCESS: {
        IssueAdviceInfo info;
        delegate_->OnIssueAdviceSuccess(info);
        break;
      }
      case MINT_TOKEN_SUCCESS: {
        delegate_->OnMintTokenSuccess(kAccessToken, 3600);
        break;
      }
      case MINT_TOKEN_FAILURE: {
        GoogleServiceAuthError error(GoogleServiceAuthError::CONNECTION_FAILED);
        delegate_->OnMintTokenFailure(error);
        break;
      }
      case MINT_TOKEN_BAD_CREDENTIALS: {
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

ProfileKeyedService* IdentityAPITestFactory(Profile* profile) {
  return new IdentityAPI(profile);
}

}  // namespace

class ExperimentalMockGetAuthTokenFunction :
    public ExperimentalIdentityGetAuthTokenFunction {
 public:
  ExperimentalMockGetAuthTokenFunction()
      : login_ui_result_(true),
        install_ui_result_(false),
        login_ui_shown_(false),
        install_ui_shown_(false) {}

  void set_login_ui_result(bool result) { login_ui_result_ = result; }

  void set_install_ui_result(bool result) { install_ui_result_ = result; }

  bool login_ui_shown() const { return login_ui_shown_; }

  bool install_ui_shown() const { return install_ui_shown_; }

  virtual void ShowLoginPopup() OVERRIDE {
    EXPECT_FALSE(login_ui_shown_);
    login_ui_shown_ = true;
    if (login_ui_result_)
      SigninSuccess("fake_refresh_token");
    else
      SigninFailed();
  }

  virtual void ShowOAuthApprovalDialog(const IssueAdviceInfo& issue_advice)
      OVERRIDE {
    install_ui_shown_ = true;
    // Call InstallUIProceed or InstallUIAbort based on the flag.
    if (install_ui_result_)
      InstallUIProceed();
    else
      InstallUIAbort(true);
  }

  MOCK_CONST_METHOD0(HasLoginToken, bool());
  MOCK_METHOD1(CreateMintTokenFlow,
               OAuth2MintTokenFlow*(OAuth2MintTokenFlow::Mode mode));

 private:
  ~ExperimentalMockGetAuthTokenFunction() {}
  bool login_ui_result_;
  bool install_ui_result_;
  bool login_ui_shown_;
  bool install_ui_shown_;
};

class ExperimentalGetAuthTokenFunctionTest : public AsyncExtensionBrowserTest {
 protected:
  enum OAuth2Fields {
    NONE = 0,
    CLIENT_ID = 1,
    SCOPES = 2
  };

  virtual ~ExperimentalGetAuthTokenFunctionTest() {}

  // Helper to create an extension with specific OAuth2Info fields set.
  // |fields_to_set| should be computed by using fields of Oauth2Fields enum.
  const Extension* CreateExtension(int fields_to_set) {
    const Extension* ext =
        LoadExtension(test_data_dir_.AppendASCII("platform_apps/oauth2"));
    OAuth2Info& oauth2_info =
        const_cast<OAuth2Info&>(OAuth2Info::GetOAuth2Info(ext));
    if ((fields_to_set & CLIENT_ID) != 0)
      oauth2_info.client_id = "client1";
    if ((fields_to_set & SCOPES) != 0) {
      oauth2_info.scopes.push_back("scope1");
      oauth2_info.scopes.push_back("scope2");
    }
    return ext;
  }

  IdentityAPI* id_api() {
    return IdentityAPI::GetFactoryInstance()->GetForProfile(
        browser()->profile());
  }
};

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest, NoClientId) {
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  func->set_extension(CreateExtension(SCOPES));
  std::string error =
      utils::RunFunctionAndReturnError(func.get(), "[{}]", browser());
  EXPECT_EQ(std::string(errors::kInvalidClientId), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest, NoScopes) {
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID));
  std::string error =
      utils::RunFunctionAndReturnError(func.get(), "[{}]", browser());
  EXPECT_EQ(std::string(errors::kInvalidScopes), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest,
                       NonInteractiveNotSignedIn) {
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken()).WillOnce(Return(false));
  std::string error =
      utils::RunFunctionAndReturnError(func.get(), "[{}]", browser());
  EXPECT_EQ(std::string(errors::kUserNotSignedIn), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest,
                       NonInteractiveMintFailure) {
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken()).WillOnce(Return(true));
  TestOAuth2MintTokenFlow* flow = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::MINT_TOKEN_FAILURE, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_)).WillOnce(Return(flow));
  std::string error =
      utils::RunFunctionAndReturnError(func.get(), "[{}]", browser());
  EXPECT_TRUE(StartsWithASCII(error, errors::kAuthFailure, false));
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest,
                       NonInteractiveMintAdviceSuccess) {
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  func->set_extension(extension);
  EXPECT_CALL(*func.get(), HasLoginToken()).WillOnce(Return(true));
  TestOAuth2MintTokenFlow* flow = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_)).WillOnce(Return(flow));
  std::string error =
      utils::RunFunctionAndReturnError(func.get(), "[{}]", browser());
  EXPECT_EQ(std::string(errors::kNoGrant), error);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());

  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(extension);
  EXPECT_EQ(
      IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
      id_api()->GetCachedToken(extension->id(), oauth2_info.scopes).status());
}

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest,
                       NonInteractiveMintBadCredentials) {
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken()).WillOnce(Return(true));
  TestOAuth2MintTokenFlow* flow = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::MINT_TOKEN_BAD_CREDENTIALS, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_)).WillOnce(Return(flow));
  std::string error =
      utils::RunFunctionAndReturnError(func.get(), "[{}]", browser());
  EXPECT_TRUE(StartsWithASCII(error, errors::kAuthFailure, false));
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest,
                       NonInteractiveSuccess) {
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension);
  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(extension);
  EXPECT_CALL(*func.get(), HasLoginToken()).WillOnce(Return(true));
  TestOAuth2MintTokenFlow* flow = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_)).WillOnce(Return(flow));
  scoped_ptr<base::Value> value(
      utils::RunFunctionAndReturnSingleResult(func.get(), "[{}]", browser()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
  EXPECT_EQ(
      IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
      id_api()->GetCachedToken(extension->id(), oauth2_info.scopes).status());
}

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest,
                       InteractiveLoginCanceled) {
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken()).WillOnce(Return(false));
  func->set_login_ui_result(false);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_EQ(std::string(errors::kUserNotSignedIn), error);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest,
                       InteractiveMintBadCredentialsLoginCanceled) {
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken()).WillOnce(Return(true));
  TestOAuth2MintTokenFlow* flow = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::MINT_TOKEN_BAD_CREDENTIALS, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_)).WillOnce(Return(flow));
  func->set_login_ui_result(false);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_EQ(std::string(errors::kUserNotSignedIn), error);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest,
                       InteractiveLoginSuccessNoToken) {
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken()).WillOnce(Return(false));
  func->set_login_ui_result(false);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_EQ(std::string(errors::kUserNotSignedIn), error);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest,
                       InteractiveLoginSuccessMintFailure) {
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken()).WillOnce(Return(false));
  func->set_login_ui_result(true);
  TestOAuth2MintTokenFlow* flow = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::MINT_TOKEN_FAILURE, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_)).WillOnce(Return(flow));
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_TRUE(StartsWithASCII(error, errors::kAuthFailure, false));
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_FALSE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest,
                       InteractiveLoginSuccessMintSuccess) {
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken()).WillOnce(Return(false));
  func->set_login_ui_result(true);
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

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest,
                       InteractiveLoginSuccessApprovalAborted) {
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken()).WillOnce(Return(false));
  func->set_login_ui_result(true);
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

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest,
                       InteractiveLoginSuccessApprovalDoneMintFailure) {
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken()).WillOnce(Return(false));
  func->set_login_ui_result(true);
  TestOAuth2MintTokenFlow* flow1 = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS, func.get());
  TestOAuth2MintTokenFlow* flow2 = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::MINT_TOKEN_FAILURE, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_)).WillOnce(Return(flow1))
      .WillOnce(Return(flow2));

  func->set_install_ui_result(true);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_TRUE(StartsWithASCII(error, errors::kAuthFailure, false));
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_TRUE(func->install_ui_shown());
}

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest,
                       InteractiveLoginSuccessApprovalDoneMintSuccess) {
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  scoped_refptr<const Extension> extension(CreateExtension(CLIENT_ID | SCOPES));
  func->set_extension(extension);
  const OAuth2Info& oauth2_info = OAuth2Info::GetOAuth2Info(extension);
  EXPECT_CALL(*func.get(), HasLoginToken()).WillOnce(Return(false));
  func->set_login_ui_result(true);
  TestOAuth2MintTokenFlow* flow1 = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS, func.get());
  TestOAuth2MintTokenFlow* flow2 = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_)).WillOnce(Return(flow1))
      .WillOnce(Return(flow2));

  func->set_install_ui_result(true);
  scoped_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      func.get(), "[{\"interactive\": true}]", browser()));
  std::string access_token;
  EXPECT_TRUE(value->GetAsString(&access_token));
  EXPECT_EQ(std::string(kAccessToken), access_token);
  EXPECT_TRUE(func->login_ui_shown());
  EXPECT_TRUE(func->install_ui_shown());
  EXPECT_EQ(
      IdentityTokenCacheValue::CACHE_STATUS_NOTFOUND,
      id_api()->GetCachedToken(extension->id(), oauth2_info.scopes).status());
}

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest,
                       InteractiveApprovalAborted) {
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken()).WillOnce(Return(true));
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

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest,
                       InteractiveApprovalDoneMintSuccess) {
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken()).WillOnce(Return(true));
  TestOAuth2MintTokenFlow* flow1 = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS, func.get());
  TestOAuth2MintTokenFlow* flow2 = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::MINT_TOKEN_SUCCESS, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_)).WillOnce(Return(flow1))
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

IN_PROC_BROWSER_TEST_F(ExperimentalGetAuthTokenFunctionTest,
                       InteractiveApprovalDoneMintBadCredentials) {
  scoped_refptr<ExperimentalMockGetAuthTokenFunction> func(
      new ExperimentalMockGetAuthTokenFunction());
  func->set_extension(CreateExtension(CLIENT_ID | SCOPES));
  EXPECT_CALL(*func.get(), HasLoginToken()).WillOnce(Return(true));
  TestOAuth2MintTokenFlow* flow1 = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::ISSUE_ADVICE_SUCCESS, func.get());
  TestOAuth2MintTokenFlow* flow2 = new TestOAuth2MintTokenFlow(
      TestOAuth2MintTokenFlow::MINT_TOKEN_BAD_CREDENTIALS, func.get());
  EXPECT_CALL(*func.get(), CreateMintTokenFlow(_)).WillOnce(Return(flow1))
      .WillOnce(Return(flow2));

  func->set_install_ui_result(true);
  std::string error = utils::RunFunctionAndReturnError(
      func.get(), "[{\"interactive\": true}]", browser());
  EXPECT_TRUE(StartsWithASCII(error, errors::kAuthFailure, false));
  EXPECT_FALSE(func->login_ui_shown());
  EXPECT_TRUE(func->install_ui_shown());
}

class ExperimentalLaunchWebAuthFlowFunctionTest :
    public AsyncExtensionBrowserTest {
 protected:
  void RunAndCheckBounds(const std::string& extra_params,
                         int expected_x,
                         int expected_y,
                         int expected_width,
                         int expected_height) {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_BROWSER_WINDOW_READY,
        content::NotificationService::AllSources());

    scoped_refptr<ExperimentalIdentityLaunchWebAuthFlowFunction> function(
        new ExperimentalIdentityLaunchWebAuthFlowFunction());

    std::string args = base::StringPrintf(
        "[{\"interactive\": true, \"url\": \"data:text/html,auth\"%s%s}]",
        extra_params.length() ? "," : "",
        extra_params.c_str());

    RunFunctionAsync(function, args);

    observer.Wait();

    Browser* web_auth_flow_browser =
        content::Source<Browser>(observer.source()).ptr();
    EXPECT_EQ(expected_x, web_auth_flow_browser->override_bounds().x());
    EXPECT_EQ(expected_y, web_auth_flow_browser->override_bounds().y());
    EXPECT_EQ(expected_width, web_auth_flow_browser->override_bounds().width());
    EXPECT_EQ(expected_height,
              web_auth_flow_browser->override_bounds().height());

    web_auth_flow_browser->window()->Close();
  }
};

IN_PROC_BROWSER_TEST_F(ExperimentalLaunchWebAuthFlowFunctionTest, Bounds) {
  RunAndCheckBounds(std::string(), 0, 0, 0, 0);
  RunAndCheckBounds("\"width\": 100, \"height\": 200", 0, 0, 100, 200);
  RunAndCheckBounds("\"left\": 100, \"top\": 200", 100, 200, 0, 0);
  RunAndCheckBounds(
      "\"left\": 100, \"top\": 200, \"width\": 300, \"height\": 400",
      100,
      200,
      300,
      400);
}

IN_PROC_BROWSER_TEST_F(ExperimentalLaunchWebAuthFlowFunctionTest,
                       UserCloseWindow) {
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_BROWSER_WINDOW_READY,
      content::NotificationService::AllSources());

  scoped_refptr<ExperimentalIdentityLaunchWebAuthFlowFunction> function(
      new ExperimentalIdentityLaunchWebAuthFlowFunction());

  RunFunctionAsync(
      function, "[{\"interactive\": true, \"url\": \"data:text/html,auth\"}]");

  observer.Wait();
  Browser* web_auth_flow_browser =
      content::Source<Browser>(observer.source()).ptr();
  web_auth_flow_browser->window()->Close();

  EXPECT_EQ(std::string(errors::kUserRejected), WaitForError(function));
}

IN_PROC_BROWSER_TEST_F(ExperimentalLaunchWebAuthFlowFunctionTest,
                       InteractionRequired) {
  scoped_refptr<ExperimentalIdentityLaunchWebAuthFlowFunction> function(
      new ExperimentalIdentityLaunchWebAuthFlowFunction());
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());
  function->set_extension(empty_extension.get());

  std::string error = utils::RunFunctionAndReturnError(
      function,
      "[{\"interactive\": false, \"url\": \"data:text/html,auth\"}]",
      browser());

  EXPECT_EQ(std::string(errors::kInteractionRequired), error);
}

IN_PROC_BROWSER_TEST_F(ExperimentalLaunchWebAuthFlowFunctionTest,
                       NonInteractiveSuccess) {
  scoped_refptr<ExperimentalIdentityLaunchWebAuthFlowFunction> function(
      new ExperimentalIdentityLaunchWebAuthFlowFunction());
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());
  function->set_extension(empty_extension.get());

  function->InitFinalRedirectURLPrefixesForTest("abcdefghij");
  scoped_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      function,
      "[{\"interactive\": false,"
      "\"url\": \"https://abcdefghij.chromiumapp.org/callback#test\"}]",
      browser()));

  std::string url;
  EXPECT_TRUE(value->GetAsString(&url));
  EXPECT_EQ(std::string("https://abcdefghij.chromiumapp.org/callback#test"),
            url);
}

IN_PROC_BROWSER_TEST_F(ExperimentalLaunchWebAuthFlowFunctionTest,
                       InteractiveFirstNavigationSuccess) {
  scoped_refptr<ExperimentalIdentityLaunchWebAuthFlowFunction> function(
      new ExperimentalIdentityLaunchWebAuthFlowFunction());
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());
  function->set_extension(empty_extension.get());

  function->InitFinalRedirectURLPrefixesForTest("abcdefghij");
  scoped_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      function,
      "[{\"interactive\": true,"
      "\"url\": \"https://abcdefghij.chromiumapp.org/callback#test\"}]",
      browser()));

  std::string url;
  EXPECT_TRUE(value->GetAsString(&url));
  EXPECT_EQ(std::string("https://abcdefghij.chromiumapp.org/callback#test"),
            url);
}

IN_PROC_BROWSER_TEST_F(ExperimentalLaunchWebAuthFlowFunctionTest,
                       InteractiveSecondNavigationSuccess) {
  scoped_refptr<ExperimentalIdentityLaunchWebAuthFlowFunction> function(
      new ExperimentalIdentityLaunchWebAuthFlowFunction());
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());
  function->set_extension(empty_extension.get());

  function->InitFinalRedirectURLPrefixesForTest("abcdefghij");
  scoped_ptr<base::Value> value(utils::RunFunctionAndReturnSingleResult(
      function,
      "[{\"interactive\": true,"
      "\"url\": \"data:text/html,<script>window.location.replace('"
      "https://abcdefghij.chromiumapp.org/callback#test')</script>\"}]",
      browser()));

  std::string url;
  EXPECT_TRUE(value->GetAsString(&url));
  EXPECT_EQ(std::string("https://abcdefghij.chromiumapp.org/callback#test"),
            url);
}

}  // namespace extensions
