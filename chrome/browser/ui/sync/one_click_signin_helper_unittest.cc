// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_manager_fake.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/test/test_browser_context.h"
#include "content/test/test_browser_thread.h"
#include "content/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock-actions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class OneClickSigninHelperTest : public testing::Test {
 public:
  OneClickSigninHelperTest() : signin_manager_(NULL) {
  }

  virtual void TearDown() OVERRIDE;

 protected:
  // Marks the current thread as the UI thread, so that calls that assume
  // they are called on that thread work.
  void MarkCurrentThreadAsUIThread();

  // Creates a mock WebContents for tests.  If |use_incognito| is true then
  // a WebContents for an incognito profile is created.
  content::WebContents* CreateMockWebContents(bool use_incognito);

  void EnableOneClick(bool enable);

  void AllowSigninCookies(bool enable);

  // Marks the profile as connected to the given account.  If |username| is the
  // empty string, the profile is not connected.
  void ConnectProfileToAccount(const std::string& username);

 private:
  // Members to fake that we are on the UI thread.
  MessageLoop message_loop_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;

  // Mock objects used during tests.  The objects need to be torn down in the
  // correct order, see TearDown().
  scoped_ptr<content::WebContents> web_contents_;
  scoped_ptr<TestingProfile> profile_;
  SigninManager* signin_manager_;

  DISALLOW_COPY_AND_ASSIGN(OneClickSigninHelperTest);
};

void OneClickSigninHelperTest::TearDown() {
  // Destroy things in proper order.
  web_contents_.reset();
  profile_.reset();
  ui_thread_.reset();
}

void OneClickSigninHelperTest::MarkCurrentThreadAsUIThread() {
  ui_thread_.reset(new content::TestBrowserThread(
      content::BrowserThread::UI, &message_loop_));
}

content::WebContents* OneClickSigninHelperTest::CreateMockWebContents(
    bool use_incognito) {
  EXPECT_TRUE(web_contents_.get() == NULL);

  profile_.reset(new TestingProfile());
  signin_manager_ = static_cast<SigninManager*>(
      SigninManagerFactory::GetInstance()->SetTestingFactoryAndUse(
          profile_.get(), FakeSigninManager::Build));

  profile_->set_incognito(use_incognito);
  web_contents_.reset(content::WebContentsTester::CreateTestWebContents(
      profile_.get(), NULL));
  return web_contents_.get();
}

void OneClickSigninHelperTest::EnableOneClick(bool enable) {
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetBoolean(prefs::kReverseAutologinEnabled, enable);
}

void OneClickSigninHelperTest::AllowSigninCookies(bool enable) {
  CookieSettings* cookie_settings =
      CookieSettings::Factory::GetForProfile(profile_.get());
  cookie_settings->SetDefaultCookieSetting(
      enable ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}

void OneClickSigninHelperTest::ConnectProfileToAccount(
    const std::string& username) {
  signin_manager_->StartSignIn(username, std::string(), std::string(),
                               std::string());
}

} // namespace

TEST_F(OneClickSigninHelperTest, CanOfferNoContents) {
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(NULL, true));
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(NULL, false));
}

TEST_F(OneClickSigninHelperTest, CanOffer) {
  MarkCurrentThreadAsUIThread();
  content::WebContents* web_contents = CreateMockWebContents(false);

  EnableOneClick(true);
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(web_contents, true));
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(web_contents, false));

  EnableOneClick(false);
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents, true));
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents, false));
}

TEST_F(OneClickSigninHelperTest, CanOfferProfileConnected) {
  MarkCurrentThreadAsUIThread();
  content::WebContents* web_contents = CreateMockWebContents(false);
  ConnectProfileToAccount("foo@gmail.com");

  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents, true));
  EXPECT_TRUE(OneClickSigninHelper::CanOffer(web_contents, false));
}

TEST_F(OneClickSigninHelperTest, CanOfferIncognito) {
  MarkCurrentThreadAsUIThread();
  content::WebContents* web_contents = CreateMockWebContents(true);

  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents, true));
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents, false));
}

TEST_F(OneClickSigninHelperTest, CanOfferNoSigninCookies) {
  MarkCurrentThreadAsUIThread();
  content::WebContents* web_contents = CreateMockWebContents(true);
  AllowSigninCookies(false);

  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents, true));
  EXPECT_FALSE(OneClickSigninHelper::CanOffer(web_contents, false));
}
