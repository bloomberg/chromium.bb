// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_manager_fake.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::Values;

namespace {

class SigninManagerMock : public FakeSigninManager {
 public:
  explicit SigninManagerMock(Profile* profile)
      : FakeSigninManager(profile) {}
  MOCK_CONST_METHOD1(IsAllowedUsername, bool(const std::string& username));
};

class OneClickSigninHelperTest : public content::RenderViewHostTestHarness {
 public:
  OneClickSigninHelperTest();

  virtual void SetUp() OVERRIDE;

 protected:
  // Creates the sign-in manager for tests.  If |use_incognito| is true then
  // a WebContents for an incognito profile is created.  If |username| is
  // is not empty, the profile of the mock WebContents will be connected to
  // the given account.
  void CreateSigninManager(bool use_incognito, const std::string& username);

  void AddEmailToOneClickRejectedList(const std::string& email);
  void EnableOneClick(bool enable);

  void AllowSigninCookies(bool enable);

  SigninManagerMock* signin_manager_;

 private:
  // Members to fake that we are on the UI thread.
  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninHelperTest);
};

OneClickSigninHelperTest::OneClickSigninHelperTest()
    : ui_thread_(content::BrowserThread::UI, &message_loop_) {
}

void OneClickSigninHelperTest::SetUp() {
  TestingProfile* testing_profile = new TestingProfile();
  browser_context_.reset(testing_profile);
  content::RenderViewHostTestHarness::SetUp();
}

class OneClickTestProfileSyncService : public TestProfileSyncService {
  public:
   virtual ~OneClickTestProfileSyncService() {}

   // Helper routine to be used in conjunction with
   // ProfileKeyedServiceFactory::SetTestingFactory().
   static ProfileKeyedService* Build(Profile* profile) {
     return new OneClickTestProfileSyncService(profile);
   }

   // Need to control this for certain tests.
   virtual bool FirstSetupInProgress() const OVERRIDE {
     return first_setup_in_progress_;
   }

   // Controls return value of FirstSetupInProgress. Because some bits
   // of UI depend on that value, it's useful to control it separately
   // from the internal work and components that are triggered (such as
   // ReconfigureDataTypeManager) to facilitate unit tests.
   void set_first_setup_in_progress(bool in_progress) {
     first_setup_in_progress_ = in_progress;
   }

   // Override ProfileSyncService::Shutdown() to avoid CHECK on
   // |invalidator_registrar_|.
   void Shutdown() OVERRIDE {};

  private:
   explicit OneClickTestProfileSyncService(Profile* profile)
       : TestProfileSyncService(NULL,
                                profile,
                                NULL,
                                ProfileSyncService::MANUAL_START,
                                false,  // synchronous_backend_init
                                base::Closure()),
         first_setup_in_progress_(false) {}

   bool first_setup_in_progress_;
};

static ProfileKeyedService* BuildSigninManagerMock(Profile* profile) {
  return new SigninManagerMock(profile);
}

void OneClickSigninHelperTest::CreateSigninManager(
    bool use_incognito,
    const std::string& username) {
  TestingProfile* testing_profile = static_cast<TestingProfile*>(
      browser_context_.get());
  testing_profile->set_incognito(use_incognito);
  signin_manager_ = static_cast<SigninManagerMock*>(
      SigninManagerFactory::GetInstance()->SetTestingFactoryAndUse(
          testing_profile, BuildSigninManagerMock));

  if (!username.empty()) {
    signin_manager_->StartSignIn(username, std::string(), std::string(),
                                std::string());
  }
}

void OneClickSigninHelperTest::EnableOneClick(bool enable) {
  PrefService* pref_service = Profile::FromBrowserContext(
      browser_context_.get())->GetPrefs();
  pref_service->SetBoolean(prefs::kReverseAutologinEnabled, enable);
}

void OneClickSigninHelperTest::AddEmailToOneClickRejectedList(
    const std::string& email) {
  PrefService* pref_service = Profile::FromBrowserContext(
      browser_context_.get())->GetPrefs();
  ListPrefUpdate updater(pref_service,
                         prefs::kReverseAutologinRejectedEmailList);
  updater->AppendIfNotPresent(Value::CreateStringValue(email));
}

void OneClickSigninHelperTest::AllowSigninCookies(bool enable) {
  CookieSettings* cookie_settings =
      CookieSettings::Factory::GetForProfile(
          Profile::FromBrowserContext(browser_context_.get()));
  cookie_settings->SetDefaultCookieSetting(
      enable ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}

}  // namespace

TEST_F(OneClickSigninHelperTest, CanOfferNoContents) {
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(NULL, "user@gmail.com", true));
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(NULL, "", false));
}

TEST_F(OneClickSigninHelperTest, CanOffer) {
  CreateSigninManager(false, "");

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
        WillRepeatedly(Return(true));

  EnableOneClick(true);
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(web_contents(), "user@gmail.com",
                                             true));
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(web_contents(), "", false));

  EnableOneClick(false);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents(), "user@gmail.com",
                                              true));
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents(), "", false));
}

TEST_F(OneClickSigninHelperTest, CanOfferFirstSetup) {
  CreateSigninManager(false, "");

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
        WillRepeatedly(Return(true));

   // Invoke OneClickTestProfileSyncService factory function and grab result.
   OneClickTestProfileSyncService* sync =
       static_cast<OneClickTestProfileSyncService*>(
           ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
               static_cast<Profile*>(browser_context()),
               OneClickTestProfileSyncService::Build));

   sync->set_first_setup_in_progress(true);

   EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents(),
                                               "foo@gmail.com",
                                               true));
   EXPECT_TRUE(OneClickSigninHelper::CanOffer(web_contents(),
                                              "foo@gmail.com",
                                              false));
}

TEST_F(OneClickSigninHelperTest, CanOfferProfileConnected) {
  CreateSigninManager(false, "foo@gmail.com");

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
      WillRepeatedly(Return(true));

  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents(),
                                              "foo@gmail.com",
                                              true));
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents(),
                                              "user@gmail.com",
                                              true));
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(web_contents(),
                                             "",
                                             false));
}

TEST_F(OneClickSigninHelperTest, CanOfferUsernameNotAllowed) {
  CreateSigninManager(false, "foo@gmail.com");

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
      WillRepeatedly(Return(false));

  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents(),
                                              "foo@gmail.com",
                                              true));
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(web_contents(),
                                             "",
                                             false));
}

TEST_F(OneClickSigninHelperTest, CanOfferWithRejectedEmail) {
  CreateSigninManager(false, "");

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
        WillRepeatedly(Return(true));

  AddEmailToOneClickRejectedList("foo@gmail.com");
  AddEmailToOneClickRejectedList("user@gmail.com");
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents(), "foo@gmail.com",
                                              true));
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents(), "user@gmail.com",
                                              true));
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(web_contents(), "john@gmail.com",
                                              true));
}

TEST_F(OneClickSigninHelperTest, CanOfferIncognito) {
  CreateSigninManager(true, "");

  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents(), "user@gmail.com",
                                              true));
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents(), "", false));
}

TEST_F(OneClickSigninHelperTest, CanOfferNoSigninCookies) {
  CreateSigninManager(false, "");
  AllowSigninCookies(false);

  EXPECT_CALL(*signin_manager_, IsAllowedUsername(_)).
        WillRepeatedly(Return(true));

  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents(), "user@gmail.com",
                                              true));
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents(), "", false));
}
