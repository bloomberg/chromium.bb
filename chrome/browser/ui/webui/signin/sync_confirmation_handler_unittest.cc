// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/sync_confirmation_handler.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/account_fetcher_service_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/fake_account_fetcher_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/dialog_test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/signin/core/browser/account_fetcher_service.h"
#include "components/signin/core/browser/fake_account_fetcher_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "content/public/test/test_web_ui.h"

const int kExpectedProfileImageSize = 128;

class TestingSyncConfirmationHandler : public SyncConfirmationHandler {
 public:
  explicit TestingSyncConfirmationHandler(content::WebUI* web_ui) {
    set_web_ui(web_ui);
  }

  using SyncConfirmationHandler::HandleConfirm;
  using SyncConfirmationHandler::HandleUndo;
  using SyncConfirmationHandler::HandleInitialized;
  using SyncConfirmationHandler::HandleGoToSettings;
  using SyncConfirmationHandler::SetUserImageURL;
};

class SyncConfirmationHandlerTest : public BrowserWithTestWindowTest {
 public:
  SyncConfirmationHandlerTest() : web_ui_(new content::TestWebUI) {}
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    chrome::NewTab(browser());
    web_ui()->set_web_contents(
        browser()->tab_strip_model()->GetActiveWebContents());

    // WebUI owns the handlers.
    handler_ = new TestingSyncConfirmationHandler(web_ui());
    sync_confirmation_ui_.reset(
        new SyncConfirmationUI(web_ui(), handler_));

    // This dialog assumes the signin flow was completed, which kicks off the
    // SigninManager.
    new OneClickSigninSyncStarter(
        profile(),
        browser(),
        "gaia",
        "foo@example.com",
        "password",
        "refresh_token",
        OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS,
        nullptr,
        OneClickSigninSyncStarter::NO_CONFIRMATION,
        GURL(),
        GURL(),
        OneClickSigninSyncStarter::Callback());
  }

  void TearDown() override {
    sync_confirmation_ui_.reset();
    web_ui_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  TestingSyncConfirmationHandler* handler() {
    return handler_;
  }

  content::TestWebUI* web_ui() {
    return web_ui_.get();
  }

  FakeAccountFetcherService* account_fetcher_service() {
    return static_cast<FakeAccountFetcherService*>(
        AccountFetcherServiceFactory::GetForProfile(profile()));
  }

  FakeSigninManager* signin_manager() {
    return static_cast<FakeSigninManager*>(
        SigninManagerFactory::GetForProfile(profile()));
  }

  ProfileSyncService* sync() {
    return ProfileSyncServiceFactory::GetForProfile(profile());
  }

  // BrowserWithTestWindowTest
  BrowserWindow* CreateBrowserWindow() override {
    return new DialogTestBrowserWindow;
  }

  TestingProfile* CreateProfile() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(AccountFetcherServiceFactory::GetInstance(),
                              FakeAccountFetcherServiceBuilder::BuildForTests);
    builder.AddTestingFactory(
        SigninManagerFactory::GetInstance(), BuildFakeSigninManagerBase);
    return builder.Build().release();
  }

private:
  scoped_ptr<content::TestWebUI> web_ui_;
  scoped_ptr<SyncConfirmationUI> sync_confirmation_ui_;
  TestingSyncConfirmationHandler* handler_;  // Not owned.
};

TEST_F(SyncConfirmationHandlerTest, TestSetImageIfPrimaryAccountReady) {
  account_fetcher_service()->FakeUserInfoFetchSuccess(
      "gaia",
      "foo@example.com",
      "gaia",
      "",
      "full_name",
      "given_name",
      "locale",
      "http://picture.example.com/picture.jpg");

  handler()->HandleInitialized(nullptr);
  EXPECT_EQ(1U, web_ui()->call_data().size());
  EXPECT_EQ("sync.confirmation.setUserImageURL",
            web_ui()->call_data()[0]->function_name());
  EXPECT_TRUE(
      web_ui()->call_data()[0]->arg1()->IsType(base::Value::TYPE_STRING));
  std::string passed_picture_url;
  EXPECT_TRUE(
      web_ui()->call_data()[0]->arg1()->GetAsString(&passed_picture_url));

  std::string original_picture_url =
      AccountTrackerServiceFactory::GetForProfile(profile())->
          GetAccountInfo("gaia").picture_url;
  GURL picture_url_with_size;
  EXPECT_TRUE(profiles::GetImageURLWithThumbnailSize(GURL(original_picture_url),
                                                     kExpectedProfileImageSize,
                                                     &picture_url_with_size));
  EXPECT_EQ(picture_url_with_size.spec(), passed_picture_url);
  handler()->HandleUndo(nullptr);
}

TEST_F(SyncConfirmationHandlerTest, TestSetImageIfPrimaryAccountReadyLater) {
  handler()->HandleInitialized(nullptr);
  EXPECT_EQ(0U, web_ui()->call_data().size());

  account_fetcher_service()->FakeUserInfoFetchSuccess(
      "gaia",
      "foo@example.com",
      "gaia",
      "",
      "full_name",
      "given_name",
      "locale",
      "http://picture.example.com/picture.jpg");

  EXPECT_EQ(1U, web_ui()->call_data().size());
  EXPECT_EQ("sync.confirmation.setUserImageURL",
            web_ui()->call_data()[0]->function_name());
  EXPECT_TRUE(
      web_ui()->call_data()[0]->arg1()->IsType(base::Value::TYPE_STRING));
  std::string passed_picture_url;
  EXPECT_TRUE(
      web_ui()->call_data()[0]->arg1()->GetAsString(&passed_picture_url));

  std::string original_picture_url =
      AccountTrackerServiceFactory::GetForProfile(profile())->
          GetAccountInfo("gaia").picture_url;
  GURL picture_url_with_size;
  EXPECT_TRUE(profiles::GetImageURLWithThumbnailSize(GURL(original_picture_url),
                                                     kExpectedProfileImageSize,
                                                     &picture_url_with_size));
  EXPECT_EQ(picture_url_with_size.spec(), passed_picture_url);
  handler()->HandleUndo(nullptr);
}

TEST_F(SyncConfirmationHandlerTest, TestHandleUndo) {
  EXPECT_FALSE(sync()->IsFirstSetupComplete());
  EXPECT_TRUE(sync()->IsFirstSetupInProgress());

  handler()->HandleUndo(nullptr);

  EXPECT_FALSE(sync()->IsFirstSetupInProgress());
  EXPECT_FALSE(sync()->IsFirstSetupComplete());
  EXPECT_FALSE(
      SigninManagerFactory::GetForProfile(profile())->IsAuthenticated());
}

TEST_F(SyncConfirmationHandlerTest, TestHandleConfirm) {
  EXPECT_FALSE(sync()->IsFirstSetupComplete());
  EXPECT_TRUE(sync()->IsFirstSetupInProgress());

  handler()->HandleConfirm(nullptr);

  EXPECT_FALSE(sync()->IsFirstSetupInProgress());
  EXPECT_TRUE(sync()->IsFirstSetupComplete());
  EXPECT_TRUE(
      SigninManagerFactory::GetForProfile(profile())->IsAuthenticated());
}
