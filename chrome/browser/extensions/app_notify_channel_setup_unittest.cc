// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/extensions/app_notify_channel_setup.h"
#include "chrome/browser/extensions/app_notify_channel_ui.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/signin/token_service_unittest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "content/test/test_url_fetcher_factory.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using testing::_;
using testing::Return;

namespace {

const int kRouteId = 4;
const int kCallbackId = 5;
const char* kFakeExtensionId = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

static const char kValidAccessTokenResponse[] =
    "{"
    "  \"access_token\": \"at1\","
    "  \"expires_in\": 3600,"
    "  \"token_type\": \"Bearer\""
    "}";

class MockTokenService : public TokenService {
 public:
  MockTokenService() : mockToken_("test_refresh_token") { }
  virtual ~MockTokenService() { }

  bool AreCredentialsValid() const OVERRIDE {
    return true;
  }

  const std::string& GetOAuth2LoginRefreshToken() const OVERRIDE {
    return mockToken_;
  }

  std::string mockToken_;

  MOCK_CONST_METHOD0(HasOAuthLoginToken, bool());
};

ProfileKeyedService* BuildMockTokenService(Profile* profile) {
  return new MockTokenService;
}

MockTokenService* BuildForProfile(Profile* profile) {
  return static_cast<MockTokenService*>(
      TokenServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile, BuildMockTokenService));
}

class TestProfile : public TestingProfile {
 public:
  TestProfile()
      : TestingProfile(),
        token_service_(BuildForProfile(this)) {
  }

  virtual ~TestProfile() { }

  void SetTokenServiceHasTokenResult(bool result) {
    EXPECT_CALL(*token_service_, HasOAuthLoginToken())
        .WillRepeatedly(Return(result));
  }

 private:
  MockTokenService* token_service_;
};

class TestDelegate : public AppNotifyChannelSetup::Delegate,
                     public base::SupportsWeakPtr<TestDelegate> {
 public:
  TestDelegate() : was_called_(false) {}
  virtual ~TestDelegate() {}

  virtual void AppNotifyChannelSetupComplete(
      const std::string& channel_id,
      const std::string& error,
      const AppNotifyChannelSetup* setup) OVERRIDE {
    EXPECT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    EXPECT_FALSE(was_called_);
    was_called_ = true;
    error_ = error;
    route_id_ = setup->return_route_id();
    callback_id_ = setup->callback_id();
    MessageLoop::current()->Quit();
  }

  // Called to check that we were called with the expected arguments.
  void ExpectWasCalled(const std::string& expected_channel_id,
                       const std::string& expected_error) {
    EXPECT_TRUE(was_called_);
    EXPECT_EQ(expected_error, error_);
    EXPECT_EQ(kRouteId, route_id_);
    EXPECT_EQ(kCallbackId, callback_id_);
  }

 private:
  // Has our callback been called yet?
  bool was_called_;

  // When our AppNotifyChannelSetupComplete method is called, we copy the
  // arguments into these member variables.
  std::string channel_id_;
  std::string error_;
  int route_id_;
  int callback_id_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class TestUI : public AppNotifyChannelUI {
 public:
  TestUI() : delegate_(NULL) {}
  ~TestUI() {}

  // AppNotifyChannelUI.
  virtual void PromptSyncSetup(Delegate* delegate) OVERRIDE {
    CHECK(!delegate_);
    delegate_ = delegate;

    // If we have a result, post a task to call back the delegate with
    // it. Otherwise ReportResult can be called manually at some later point.
    if (setup_result_.get()) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&TestUI::ReportResult,
                     base::Unretained(this),
                     *setup_result_));
    }
  }

  // This will make us automatically call back the delegate with |result| after
  // PromptSyncSetup is called.
  void SetSyncSetupResult(bool result) {
    setup_result_.reset(new bool);
    *setup_result_ = result;
  }

  void ReportResult(bool result) {
    CHECK(delegate_);
    delegate_->OnSyncSetupResult(result);
  }

 private:
  AppNotifyChannelUI::Delegate* delegate_;
  scoped_ptr<bool> setup_result_;

  DISALLOW_COPY_AND_ASSIGN(TestUI);
};

}  // namespace

class AppNotifyChannelSetupTest : public testing::Test {
 public:
  AppNotifyChannelSetupTest() : ui_thread_(BrowserThread::UI, &message_loop_),
                                ui_(new TestUI()) {
  }

  virtual ~AppNotifyChannelSetupTest() {}

  virtual void SetLoggedInUser(const std::string username) {
    TestingPrefService* prefs = profile_.GetTestingPrefService();
    prefs->SetUserPref(prefs::kGoogleServicesUsername,
                       new StringValue(username));
  }

  virtual AppNotifyChannelSetup* CreateInstance() {
    GURL page_url("http://www.google.com");
    return new AppNotifyChannelSetup(&profile_,
                                     kFakeExtensionId,
                                     "1234",
                                     page_url,
                                     kRouteId,
                                     kCallbackId,
                                     ui_.release(),
                                     delegate_.AsWeakPtr());
  }

  virtual void SetupLogin(bool should_prompt, bool should_succeed) {
    if (should_succeed) {
      SetLoggedInUser("user@gmail.com");
      profile_.SetTokenServiceHasTokenResult(true);
    }
    if (should_prompt)
      ui_->SetSyncSetupResult(should_succeed);
  }

  virtual void SetupFetchAccessToken(bool should_succeed) {
    factory_.SetFakeResponse(
        GaiaUrls::GetInstance()->oauth2_token_url(),
        kValidAccessTokenResponse,
        should_succeed);
  }

  virtual void SetupRecordGrant(bool should_succeed) {
    factory_.SetFakeResponse(
        AppNotifyChannelSetup::GetOAuth2IssueTokenURL().spec(),
        "whatever",
        should_succeed);
  }

  virtual void SetupGetChannelId(bool should_succeed) {
    factory_.SetFakeResponse(
        AppNotifyChannelSetup::GetCWSChannelServiceURL().spec(),
        "{\"id\": \"dummy_do_not_use\"}",
        should_succeed);
  }

  virtual void RunServerTest(AppNotifyChannelSetup* setup,
                             const std::string& expected_code,
                             const std::string& expected_error) {
    setup->Start();
    message_loop_.Run();
    delegate_.ExpectWasCalled(expected_code, expected_error);
  }

 protected:
  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  TestProfile profile_;
  TestDelegate delegate_;
  scoped_ptr<TestUI> ui_;
  FakeURLFetcherFactory factory_;
};

TEST_F(AppNotifyChannelSetupTest, LoginFailure) {
  SetupLogin(true, false);

  scoped_refptr<AppNotifyChannelSetup> setup = CreateInstance();
  RunServerTest(setup, "", "canceled_by_user");
}

TEST_F(AppNotifyChannelSetupTest, DoubleFetchAccessTokenFailure) {
  SetupLogin(false, true);
  SetupFetchAccessToken(false);
  SetupLogin(true, true);
  SetupFetchAccessToken(false);

  scoped_refptr<AppNotifyChannelSetup> setup = CreateInstance();
  RunServerTest(setup, "", "internal_error");
}

TEST_F(AppNotifyChannelSetupTest, RecordGrantFailure) {
  SetupLogin(false, true);
  SetupFetchAccessToken(true);
  SetupRecordGrant(false);

  scoped_refptr<AppNotifyChannelSetup> setup = CreateInstance();
  RunServerTest(setup, "", "internal_error");
}

TEST_F(AppNotifyChannelSetupTest, GetChannelIdFailure) {
  SetupLogin(false, true);
  SetupFetchAccessToken(true);
  SetupRecordGrant(true);
  SetupGetChannelId(false);

  scoped_refptr<AppNotifyChannelSetup> setup = CreateInstance();
  RunServerTest(setup, "", "internal_error");
}

TEST_F(AppNotifyChannelSetupTest, FirstFetchAccessTokenSuccess) {
  SetupLogin(false, true);
  SetupFetchAccessToken(true);
  SetupRecordGrant(true);
  SetupGetChannelId(true);

  scoped_refptr<AppNotifyChannelSetup> setup = CreateInstance();
  RunServerTest(setup, "dummy_do_not_use", "");
}

TEST_F(AppNotifyChannelSetupTest, SecondFetchAccessTokenSuccess) {
  SetupLogin(false, true);
  SetupFetchAccessToken(false);
  SetupLogin(true, true);
  SetupFetchAccessToken(true);
  SetupRecordGrant(true);
  SetupGetChannelId(true);

  scoped_refptr<AppNotifyChannelSetup> setup = CreateInstance();
  RunServerTest(setup, "dummy_do_not_use", "");
}
