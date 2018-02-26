// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/sync_confirmation_handler.h"

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/test/user_action_tester.h"
#include "base/values.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/account_fetcher_service_factory.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/fake_account_fetcher_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/user_event_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/sync_confirmation_ui.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/dialog_test_browser_window.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/signin/core/browser/account_fetcher_service.h"
#include "components/signin/core/browser/avatar_icon_util.h"
#include "components/signin/core/browser/fake_account_fetcher_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"

const int kExpectedProfileImageSize = 128;

// The dialog needs to be initialized with a height but the actual value doesn't
// really matter in unit tests.
const double kDefaultDialogHeight = 350.0;

const std::string kGaiaID = "gaia";
const std::string kUsername = "foo@example.com";
const std::string kPassword = "password";

class TestingSyncConfirmationHandler : public SyncConfirmationHandler {
 public:
  TestingSyncConfirmationHandler(
      Browser* browser,
      content::WebUI* web_ui,
      std::unordered_map<std::string, int> string_to_grd_id_map)
      : SyncConfirmationHandler(browser, string_to_grd_id_map) {
    set_web_ui(web_ui);
  }

  using SyncConfirmationHandler::HandleConfirm;
  using SyncConfirmationHandler::HandleUndo;
  using SyncConfirmationHandler::HandleInitializedWithSize;
  using SyncConfirmationHandler::HandleGoToSettings;
  using SyncConfirmationHandler::RecordConsent;
  using SyncConfirmationHandler::SetUserImageURL;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingSyncConfirmationHandler);
};

class TestingOneClickSigninSyncStarter : public OneClickSigninSyncStarter {
 public:
  TestingOneClickSigninSyncStarter(Profile* profile,
                                   Browser* browser,
                                   const std::string& gaia_id,
                                   const std::string& email,
                                   const std::string& password,
                                   const std::string& refresh_token,
                                   signin_metrics::AccessPoint access_point,
                                   signin_metrics::Reason signin_reason,
                                   ProfileMode profile_mode,
                                   StartSyncMode start_mode,
                                   ConfirmationRequired display_confirmation,
                                   Callback callback)
      : OneClickSigninSyncStarter(profile,
                                  browser,
                                  gaia_id,
                                  email,
                                  password,
                                  refresh_token,
                                  access_point,
                                  signin_reason,
                                  profile_mode,
                                  start_mode,
                                  display_confirmation,
                                  callback) {}

 protected:
  void ShowSyncSetupSettingsSubpage() override {
    // Intentionally don't open a tab to settings.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingOneClickSigninSyncStarter);
};

// TODO(msramek): Extract this into "consent_auditor_test_utils" for reusability
// and to remove unnecessary dependencies from this test.
class FakeConsentAuditor : public consent_auditor::ConsentAuditor {
 public:
  static std::unique_ptr<KeyedService> Build(content::BrowserContext* context) {
    return std::make_unique<FakeConsentAuditor>(
        Profile::FromBrowserContext(context));
  }

  FakeConsentAuditor(Profile* profile)
      : ConsentAuditor(
            profile->GetPrefs(),
            browser_sync::UserEventServiceFactory::GetForProfile(profile),
            std::string(),
            std::string()) {}
  ~FakeConsentAuditor() override {}

  void RecordGaiaConsent(consent_auditor::Feature feature,
                         const std::vector<int>& description_grd_ids,
                         int confirmation_grd_id,
                         consent_auditor::ConsentStatus status) override {
    recorded_ids_ = description_grd_ids;
    recorded_ids_.push_back(confirmation_grd_id);
  }

  const std::vector<int>& recorded_ids() { return recorded_ids_; }

 private:
  std::vector<int> recorded_ids_;

  DISALLOW_COPY_AND_ASSIGN(FakeConsentAuditor);
};

class SyncConfirmationHandlerTest : public BrowserWithTestWindowTest {
 public:
  static const std::string kConsentText1;
  static const std::string kConsentText2;
  static const std::string kConsentText3;
  static const std::string kConsentText4;
  static const std::string kConsentText5;

  SyncConfirmationHandlerTest()
      : did_user_explicitly_interact(false), web_ui_(new content::TestWebUI) {}
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    chrome::NewTab(browser());
    web_ui()->set_web_contents(
        browser()->tab_strip_model()->GetActiveWebContents());

    auto handler = std::make_unique<TestingSyncConfirmationHandler>(
        browser(), web_ui(), GetStringToGrdIdMap());
    handler_ = handler.get();
    sync_confirmation_ui_.reset(new SyncConfirmationUI(web_ui()));
    web_ui()->AddMessageHandler(std::move(handler));

    // This dialog assumes the signin flow was completed, which kicks off the
    // SigninManager.
    new TestingOneClickSigninSyncStarter(
        profile(), browser(), "gaia", "foo@example.com", "password",
        "refresh_token", signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN,
        signin_metrics::Reason::REASON_UNKNOWN_REASON,
        OneClickSigninSyncStarter::CURRENT_PROFILE,
        OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS,
        OneClickSigninSyncStarter::NO_CONFIRMATION,
        OneClickSigninSyncStarter::Callback());
  }

  void TearDown() override {
    sync_confirmation_ui_.reset();
    web_ui_.reset();
    BrowserWithTestWindowTest::TearDown();

    if (did_user_explicitly_interact) {
      EXPECT_EQ(0, user_action_tester()->GetActionCount("Signin_Abort_Signin"));
    } else {
      EXPECT_EQ(1, user_action_tester()->GetActionCount("Signin_Abort_Signin"));
    }
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

  browser_sync::ProfileSyncService* sync() {
    return ProfileSyncServiceFactory::GetForProfile(profile());
  }

  base::UserActionTester* user_action_tester() {
    return &user_action_tester_;
  }

  FakeConsentAuditor* consent_auditor() {
    return static_cast<FakeConsentAuditor*>(
        ConsentAuditorFactory::GetForProfile(profile()));
  }

  // BrowserWithTestWindowTest
  BrowserWindow* CreateBrowserWindow() override {
    return new DialogTestBrowserWindow;
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {{AccountFetcherServiceFactory::GetInstance(),
             FakeAccountFetcherServiceBuilder::BuildForTests},
            {SigninManagerFactory::GetInstance(), BuildFakeSigninManagerBase},
            {ConsentAuditorFactory::GetInstance(), FakeConsentAuditor::Build}};
  }

  const std::unordered_map<std::string, int>& GetStringToGrdIdMap() {
    if (string_to_grd_id_map_.empty()) {
      string_to_grd_id_map_[kConsentText1] = 1;
      string_to_grd_id_map_[kConsentText2] = 2;
      string_to_grd_id_map_[kConsentText3] = 3;
      string_to_grd_id_map_[kConsentText4] = 4;
      string_to_grd_id_map_[kConsentText5] = 5;
    }
    return string_to_grd_id_map_;
  }

 protected:
  bool did_user_explicitly_interact;

 private:
  std::unique_ptr<content::TestWebUI> web_ui_;
  std::unique_ptr<SyncConfirmationUI> sync_confirmation_ui_;
  TestingSyncConfirmationHandler* handler_;  // Not owned.
  base::UserActionTester user_action_tester_;
  std::unordered_map<std::string, int> string_to_grd_id_map_;

  DISALLOW_COPY_AND_ASSIGN(SyncConfirmationHandlerTest);
};

const std::string SyncConfirmationHandlerTest::kConsentText1 = "consentText1";
const std::string SyncConfirmationHandlerTest::kConsentText2 = "consentText2";
const std::string SyncConfirmationHandlerTest::kConsentText3 = "consentText3";
const std::string SyncConfirmationHandlerTest::kConsentText4 = "consentText4";
const std::string SyncConfirmationHandlerTest::kConsentText5 = "consentText5";

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

  base::ListValue args;
  args.Set(0, std::make_unique<base::Value>(kDefaultDialogHeight));
  handler()->HandleInitializedWithSize(&args);
  EXPECT_EQ(2U, web_ui()->call_data().size());

  // When the primary account is ready, setUserImageURL happens before
  // clearFocus since the image URL is known before showing the dialog.
  EXPECT_EQ("sync.confirmation.setUserImageURL",
            web_ui()->call_data()[0]->function_name());
  EXPECT_TRUE(web_ui()->call_data()[0]->arg1()->is_string());
  std::string passed_picture_url;
  EXPECT_TRUE(
      web_ui()->call_data()[0]->arg1()->GetAsString(&passed_picture_url));

  EXPECT_EQ("sync.confirmation.clearFocus",
            web_ui()->call_data()[1]->function_name());

  std::string original_picture_url =
      AccountTrackerServiceFactory::GetForProfile(profile())->
          GetAccountInfo("gaia").picture_url;
  GURL picture_url_with_size = signin::GetAvatarImageURLWithOptions(
      GURL(original_picture_url), kExpectedProfileImageSize,
      false /* no_silhouette */);
  EXPECT_EQ(picture_url_with_size.spec(), passed_picture_url);
}

TEST_F(SyncConfirmationHandlerTest, TestSetImageIfPrimaryAccountReadyLater) {
  base::ListValue args;
  args.Set(0, std::make_unique<base::Value>(kDefaultDialogHeight));
  handler()->HandleInitializedWithSize(&args);
  EXPECT_EQ(2U, web_ui()->call_data().size());

  account_fetcher_service()->FakeUserInfoFetchSuccess(
      "gaia",
      "foo@example.com",
      "gaia",
      "",
      "full_name",
      "given_name",
      "locale",
      "http://picture.example.com/picture.jpg");

  EXPECT_EQ(3U, web_ui()->call_data().size());

  // When the primary account isn't yet ready when the dialog is shown,
  // setUserImageURL is called with the default placeholder image.
  EXPECT_EQ("sync.confirmation.setUserImageURL",
            web_ui()->call_data()[0]->function_name());
  EXPECT_TRUE(web_ui()->call_data()[0]->arg1()->is_string());
  std::string passed_picture_url;
  EXPECT_TRUE(
      web_ui()->call_data()[0]->arg1()->GetAsString(&passed_picture_url));
  EXPECT_EQ(profiles::GetPlaceholderAvatarIconUrl(), passed_picture_url);

  // When the primary account isn't yet ready when the dialog is shown,
  // clearFocus is called before the second call to setUserImageURL.
  EXPECT_EQ("sync.confirmation.clearFocus",
            web_ui()->call_data()[1]->function_name());

  EXPECT_EQ("sync.confirmation.setUserImageURL",
            web_ui()->call_data()[2]->function_name());
  EXPECT_TRUE(web_ui()->call_data()[2]->arg1()->is_string());
  EXPECT_TRUE(
      web_ui()->call_data()[2]->arg1()->GetAsString(&passed_picture_url));

  std::string original_picture_url =
      AccountTrackerServiceFactory::GetForProfile(profile())->
          GetAccountInfo("gaia").picture_url;
  GURL picture_url_with_size = signin::GetAvatarImageURLWithOptions(
      GURL(original_picture_url), kExpectedProfileImageSize,
      false /* no_silhouette */);
  EXPECT_EQ(picture_url_with_size.spec(), passed_picture_url);
}

TEST_F(SyncConfirmationHandlerTest,
       TestSetImageIgnoredIfSecondaryAccountUpdated) {
  base::ListValue args;
  args.Set(0, std::make_unique<base::Value>(kDefaultDialogHeight));
  handler()->HandleInitializedWithSize(&args);
  EXPECT_EQ(2U, web_ui()->call_data().size());

  AccountTrackerServiceFactory::GetForProfile(profile())->SeedAccountInfo(
      "bar_gaia", "bar@example.com");
  account_fetcher_service()->FakeUserInfoFetchSuccess(
      "bar_gaia", "bar@example.com", "bar_gaia", "", "bar_full_name",
      "bar_given_name", "bar_locale",
      "http://picture.example.com/bar_picture.jpg");

  // Updating the account info of a secondary account should not update the
  // image of the sync confirmation dialog.
  EXPECT_EQ(2U, web_ui()->call_data().size());

  account_fetcher_service()->FakeUserInfoFetchSuccess(
      "gaia", "foo@example.com", "gaia", "", "full_name", "given_name",
      "locale", "http://picture.example.com/picture.jpg");

  // Updating the account info of the primary account should update the
  // image of the sync confirmation dialog.
  EXPECT_EQ(3U, web_ui()->call_data().size());
  EXPECT_EQ("sync.confirmation.setUserImageURL",
            web_ui()->call_data()[2]->function_name());
}

TEST_F(SyncConfirmationHandlerTest, TestHandleUndo) {
  EXPECT_FALSE(sync()->IsFirstSetupComplete());
  EXPECT_TRUE(sync()->IsFirstSetupInProgress());

  handler()->HandleUndo(nullptr);
  did_user_explicitly_interact = true;

  EXPECT_FALSE(sync()->IsFirstSetupInProgress());
  EXPECT_FALSE(sync()->IsFirstSetupComplete());
  EXPECT_FALSE(
      SigninManagerFactory::GetForProfile(profile())->IsAuthenticated());
  EXPECT_EQ(1, user_action_tester()->GetActionCount("Signin_Undo_Signin"));
  EXPECT_EQ(0, user_action_tester()->GetActionCount(
      "Signin_Signin_WithDefaultSyncSettings"));
  EXPECT_EQ(0, user_action_tester()->GetActionCount(
      "Signin_Signin_WithAdvancedSyncSettings"));
}

TEST_F(SyncConfirmationHandlerTest, TestHandleConfirm) {
  // The consent description consists of strings 1, 2, and 4.
  base::ListValue consent_description;
  consent_description.GetList().push_back(
      base::Value(SyncConfirmationHandlerTest::kConsentText1));
  consent_description.GetList().push_back(
      base::Value(SyncConfirmationHandlerTest::kConsentText2));
  consent_description.GetList().push_back(
      base::Value(SyncConfirmationHandlerTest::kConsentText4));

  // The consent confirmation contains string 5.
  base::Value consent_confirmation(SyncConfirmationHandlerTest::kConsentText5);

  // These are passed as parameters to HandleConfirm().
  base::ListValue args;
  args.GetList().push_back(std::move(consent_description));
  args.GetList().push_back(std::move(consent_confirmation));

  EXPECT_FALSE(sync()->IsFirstSetupComplete());
  EXPECT_TRUE(sync()->IsFirstSetupInProgress());

  handler()->HandleConfirm(&args);
  did_user_explicitly_interact = true;

  EXPECT_FALSE(sync()->IsFirstSetupInProgress());
  EXPECT_TRUE(sync()->IsFirstSetupComplete());
  EXPECT_TRUE(
      SigninManagerFactory::GetForProfile(profile())->IsAuthenticated());
  EXPECT_EQ(0, user_action_tester()->GetActionCount("Signin_Undo_Signin"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(
      "Signin_Signin_WithDefaultSyncSettings"));
  EXPECT_EQ(0, user_action_tester()->GetActionCount(
      "Signin_Signin_WithAdvancedSyncSettings"));

  // The corresponding string IDs get recorded.
  std::vector<int> expected_ids = {1, 2, 4, 5};
  EXPECT_EQ(expected_ids, consent_auditor()->recorded_ids());
}

TEST_F(SyncConfirmationHandlerTest, TestHandleConfirmWithAdvancedSyncSettings) {
  // The consent description consists of strings 2, 3, and 5.
  base::ListValue consent_description;
  consent_description.GetList().push_back(
      base::Value(SyncConfirmationHandlerTest::kConsentText2));
  consent_description.GetList().push_back(
      base::Value(SyncConfirmationHandlerTest::kConsentText3));
  consent_description.GetList().push_back(
      base::Value(SyncConfirmationHandlerTest::kConsentText5));

  // The consent confirmation contains string 2.
  base::Value consent_confirmation(SyncConfirmationHandlerTest::kConsentText2);

  // These are passed as parameters to HandleGoToSettings().
  base::ListValue args;
  args.GetList().push_back(std::move(consent_description));
  args.GetList().push_back(std::move(consent_confirmation));

  EXPECT_FALSE(sync()->IsFirstSetupComplete());
  EXPECT_TRUE(sync()->IsFirstSetupInProgress());

  handler()->HandleGoToSettings(&args);
  did_user_explicitly_interact = true;

  EXPECT_FALSE(sync()->IsFirstSetupInProgress());
  EXPECT_FALSE(sync()->IsFirstSetupComplete());
  EXPECT_TRUE(
      SigninManagerFactory::GetForProfile(profile())->IsAuthenticated());
  EXPECT_EQ(0, user_action_tester()->GetActionCount("Signin_Undo_Signin"));
  EXPECT_EQ(0, user_action_tester()->GetActionCount(
                   "Signin_Signin_WithDefaultSyncSettings"));
  EXPECT_EQ(1, user_action_tester()->GetActionCount(
                   "Signin_Signin_WithAdvancedSyncSettings"));

  // The corresponding string IDs get recorded.
  std::vector<int> expected_ids = {2, 3, 5, 2};
  EXPECT_EQ(expected_ids, consent_auditor()->recorded_ids());
}
