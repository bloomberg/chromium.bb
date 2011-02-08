// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_setup_wizard.h"

#include "base/json/json_writer.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/sync/profile_sync_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_setup_flow.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/test_browser_window.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

static const char kTestUser[] = "chrome.p13n.test@gmail.com";
static const char kTestPassword[] = "passwd";
static const char kTestCaptcha[] = "pizzamyheart";
static const char kTestCaptchaUrl[] = "http://pizzamyheart/";

typedef GoogleServiceAuthError AuthError;

// A PSS subtype to inject.
class ProfileSyncServiceForWizardTest : public ProfileSyncService {
 public:
  ProfileSyncServiceForWizardTest(ProfileSyncFactory* factory, Profile* profile)
      : ProfileSyncService(factory, profile, ""),
        user_cancelled_dialog_(false) {
    RegisterPreferences();
    ResetTestStats();
  }

  virtual ~ProfileSyncServiceForWizardTest() { }

  virtual void OnUserSubmittedAuth(const std::string& username,
                                   const std::string& password,
                                   const std::string& captcha,
                                   const std::string& access_code) {
    username_ = username;
    password_ = password;
    captcha_ = captcha;
  }

  virtual void OnUserChoseDatatypes(bool sync_everything,
      const syncable::ModelTypeSet& chosen_types) {
    user_chose_data_types_ = true;
    chosen_data_types_ = chosen_types;
  }

  virtual void OnUserCancelledDialog() {
    user_cancelled_dialog_ = true;
  }

  virtual void SetPassphrase(const std::string& passphrase,
                             bool is_explicit,
                             bool is_creation) {
    passphrase_ = passphrase;
  }

  virtual string16 GetAuthenticatedUsername() const {
    return UTF8ToUTF16(username_);
  }

  void set_auth_state(const std::string& last_email,
                      const AuthError& error) {
    last_attempted_user_email_ = last_email;
    last_auth_error_ = error;
  }

  void set_passphrase_required(bool required) {
    observed_passphrase_required_ = required;
  }

  void ResetTestStats() {
    username_.clear();
    password_.clear();
    captcha_.clear();
    user_cancelled_dialog_ = false;
    user_chose_data_types_ = false;
    keep_everything_synced_ = false;
    chosen_data_types_.clear();
  }

  std::string username_;
  std::string password_;
  std::string captcha_;
  bool user_cancelled_dialog_;
  bool user_chose_data_types_;
  bool keep_everything_synced_;
  syncable::ModelTypeSet chosen_data_types_;

  std::string passphrase_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceForWizardTest);
};

class TestingProfileWithSyncService : public TestingProfile {
 public:
  TestingProfileWithSyncService() {
    sync_service_.reset(new ProfileSyncServiceForWizardTest(&factory_, this));
  }

  virtual ProfileSyncService* GetProfileSyncService() {
    return sync_service_.get();
  }
 private:
  ProfileSyncFactoryMock factory_;
  scoped_ptr<ProfileSyncService> sync_service_;
};

class TestBrowserWindowForWizardTest : public TestBrowserWindow {
 public:
  explicit TestBrowserWindowForWizardTest(Browser* browser)
      : TestBrowserWindow(browser), flow_(NULL),
        was_show_html_dialog_called_(false) {
  }

  virtual ~TestBrowserWindowForWizardTest() {
    if (flow_.get()) {
      // In real life, the handlers are destroyed by the DOMUI infrastructure,
      // which calls GetWebUIMessageHandlers to take ownership.  This does not
      // exist in our test, so we perform cleanup manually.
      std::vector<WebUIMessageHandler*> handlers;
      flow_->GetWebUIMessageHandlers(&handlers);
      // The handler contract is that they are valid for the lifetime of the
      // HTMLDialogUIDelegate, but are cleaned up after the dialog is closed
      // and/or deleted.
      flow_.reset();
      STLDeleteElements(&handlers);
    }
  }

  // We intercept this call to hijack the flow created and then use it to
  // drive the wizard as if we were the HTML page.
  virtual void ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
      gfx::NativeWindow parent_window) {
    flow_.reset(static_cast<SyncSetupFlow*>(delegate));
    was_show_html_dialog_called_ = true;
  }

  bool TestAndResetWasShowHTMLDialogCalled() {
    bool ret = was_show_html_dialog_called_;
    was_show_html_dialog_called_ = false;
    return ret;
  }

  // Simulates the user (or browser view hierarchy) closing the html dialog.
  // Handles cleaning up the delegate and associated handlers.
  void CloseDialog() {
    if (flow_.get()) {
      std::vector<WebUIMessageHandler*> handlers;
      flow_->GetWebUIMessageHandlers(&handlers);
      // The flow deletes itself here.  Don't use reset().
      flow_.release()->OnDialogClosed("");
      STLDeleteElements(&handlers);
    }
  }

  SyncSetupFlow* flow() { return flow_.get(); }

 private:
  // In real life, this is owned by the view that is opened by the browser.  We
  // mock all that out, so we need to take ownership so the flow doesn't leak.
  scoped_ptr<SyncSetupFlow> flow_;

  bool was_show_html_dialog_called_;
};

class SyncSetupWizardTest : public BrowserWithTestWindowTest {
 public:
  SyncSetupWizardTest()
      : file_thread_(BrowserThread::FILE, MessageLoop::current()),
        test_window_(NULL),
        wizard_(NULL) { }
  virtual ~SyncSetupWizardTest() { }
  virtual void SetUp() {
    set_profile(new TestingProfileWithSyncService());
    profile()->CreateBookmarkModel(false);
    // Wait for the bookmarks model to load.
    profile()->BlockUntilBookmarkModelLoaded();
    set_browser(new Browser(Browser::TYPE_NORMAL, profile()));
    test_window_ = new TestBrowserWindowForWizardTest(browser());
    set_window(test_window_);
    browser()->set_window(window());
    BrowserList::SetLastActive(browser());
    service_ = static_cast<ProfileSyncServiceForWizardTest*>(
        profile()->GetProfileSyncService());
    wizard_.reset(new SyncSetupWizard(service_));
  }

  virtual void TearDown() {
    test_window_ = NULL;
    service_ = NULL;
    wizard_.reset();
  }

  BrowserThread file_thread_;
  TestBrowserWindowForWizardTest* test_window_;
  scoped_ptr<SyncSetupWizard> wizard_;
  ProfileSyncServiceForWizardTest* service_;
};

// See http://code.google.com/p/chromium/issues/detail?id=40715 for
// why we skip the below tests on OS X.  We don't use DISABLED_ as we
// would have to change the corresponding FRIEND_TEST() declarations.

#if defined(OS_MACOSX)
#define SKIP_TEST_ON_MACOSX() \
  do { LOG(WARNING) << "Test skipped on OS X"; return; } while (0)
#else
#define SKIP_TEST_ON_MACOSX() do {} while (0)
#endif

TEST_F(SyncSetupWizardTest, InitialStepLogin) {
  SKIP_TEST_ON_MACOSX();
  DictionaryValue dialog_args;
  SyncSetupFlow::GetArgsForGaiaLogin(service_, &dialog_args);
  std::string json_start_args;
  base::JSONWriter::Write(&dialog_args, false, &json_start_args);
  ListValue credentials;
  std::string auth = "{\"user\":\"";
  auth += std::string(kTestUser) + "\",\"pass\":\"";
  auth += std::string(kTestPassword) + "\",\"captcha\":\"";
  auth += std::string(kTestCaptcha) + "\",\"access_code\":\"";
  auth += std::string() + "\"}";
  credentials.Append(new StringValue(auth));

  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_FALSE(test_window_->flow());
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);

  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_TRUE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, test_window_->flow()->current_state_);
  EXPECT_EQ(SyncSetupWizard::DONE, test_window_->flow()->end_state_);
  EXPECT_EQ(json_start_args, test_window_->flow()->dialog_start_args_);

  // Simulate the user submitting credentials.
  test_window_->flow()->flow_handler_->HandleSubmitAuth(&credentials);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, test_window_->flow()->current_state_);
  EXPECT_EQ(kTestUser, service_->username_);
  EXPECT_EQ(kTestPassword, service_->password_);
  EXPECT_EQ(kTestCaptcha, service_->captcha_);
  EXPECT_FALSE(service_->user_cancelled_dialog_);
  service_->ResetTestStats();

  // Simulate failed credentials.
  AuthError invalid_gaia(AuthError::INVALID_GAIA_CREDENTIALS);
  service_->set_auth_state(kTestUser, invalid_gaia);
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_FALSE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, test_window_->flow()->current_state_);
  dialog_args.Clear();
  SyncSetupFlow::GetArgsForGaiaLogin(service_, &dialog_args);
  EXPECT_EQ(5U, dialog_args.size());
  std::string iframe_to_show;
  dialog_args.GetString("iframeToShow", &iframe_to_show);
  EXPECT_EQ("login", iframe_to_show);
  std::string actual_user;
  dialog_args.GetString("user", &actual_user);
  EXPECT_EQ(kTestUser, actual_user);
  int error = -1;
  dialog_args.GetInteger("error", &error);
  EXPECT_EQ(static_cast<int>(AuthError::INVALID_GAIA_CREDENTIALS), error);
  service_->set_auth_state(kTestUser, AuthError::None());

  // Simulate captcha.
  AuthError captcha_error(AuthError::FromCaptchaChallenge(
      std::string(), GURL(kTestCaptchaUrl), GURL()));
  service_->set_auth_state(kTestUser, captcha_error);
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  SyncSetupFlow::GetArgsForGaiaLogin(service_, &dialog_args);
  EXPECT_EQ(5U, dialog_args.size());
  dialog_args.GetString("iframeToShow", &iframe_to_show);
  EXPECT_EQ("login", iframe_to_show);
  std::string captcha_url;
  dialog_args.GetString("captchaUrl", &captcha_url);
  EXPECT_EQ(kTestCaptchaUrl, GURL(captcha_url).spec());
  error = -1;
  dialog_args.GetInteger("error", &error);
  EXPECT_EQ(static_cast<int>(AuthError::CAPTCHA_REQUIRED), error);
  service_->set_auth_state(kTestUser, AuthError::None());

  // Simulate success.
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_FALSE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  // In a non-discrete run, GAIA_SUCCESS immediately transitions you to
  // CONFIGURE.
  EXPECT_EQ(SyncSetupWizard::CONFIGURE,
            test_window_->flow()->current_state_);

  // That's all we're testing here, just move on to DONE.  We'll test the
  // "choose data types" scenarios elsewhere.
  wizard_->Step(SyncSetupWizard::SETTING_UP);  // No merge and sync.
  wizard_->Step(SyncSetupWizard::DONE);  // No merge and sync.
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_FALSE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_EQ(SyncSetupWizard::DONE, test_window_->flow()->current_state_);
}

TEST_F(SyncSetupWizardTest, ChooseDataTypesSetsPrefs) {
  SKIP_TEST_ON_MACOSX();
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::CONFIGURE);

  ListValue data_type_choices_value;
  std::string data_type_choices = "{\"keepEverythingSynced\":false,";
  data_type_choices += "\"syncBookmarks\":true,\"syncPreferences\":true,";
  data_type_choices += "\"syncThemes\":false,\"syncPasswords\":false,";
  data_type_choices += "\"syncAutofill\":false,\"syncExtensions\":false,";
  data_type_choices += "\"syncTypedUrls\":true,\"syncApps\":true,";
  data_type_choices += "\"syncSessions\":false,\"usePassphrase\":false}";
  data_type_choices_value.Append(new StringValue(data_type_choices));

  // Simulate the user choosing data types; bookmarks, prefs, typed
  // URLS, and apps are on, the rest are off.
  test_window_->flow()->flow_handler_->HandleConfigure(
      &data_type_choices_value);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_TRUE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_FALSE(service_->keep_everything_synced_);
  EXPECT_EQ(service_->chosen_data_types_.count(syncable::BOOKMARKS), 1U);
  EXPECT_EQ(service_->chosen_data_types_.count(syncable::PREFERENCES), 1U);
  EXPECT_EQ(service_->chosen_data_types_.count(syncable::THEMES), 0U);
  EXPECT_EQ(service_->chosen_data_types_.count(syncable::PASSWORDS), 0U);
  EXPECT_EQ(service_->chosen_data_types_.count(syncable::AUTOFILL), 0U);
  EXPECT_EQ(service_->chosen_data_types_.count(syncable::EXTENSIONS), 0U);
  EXPECT_EQ(service_->chosen_data_types_.count(syncable::TYPED_URLS), 1U);
  EXPECT_EQ(service_->chosen_data_types_.count(syncable::APPS), 1U);

  test_window_->CloseDialog();
}

TEST_F(SyncSetupWizardTest, EnterPassphraseRequired) {
  SKIP_TEST_ON_MACOSX();
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::CONFIGURE);
  wizard_->Step(SyncSetupWizard::SETTING_UP);
  service_->set_passphrase_required(true);
  wizard_->Step(SyncSetupWizard::ENTER_PASSPHRASE);
  EXPECT_EQ(SyncSetupWizard::ENTER_PASSPHRASE,
            test_window_->flow()->current_state_);
  ListValue value;
  value.Append(new StringValue("{\"passphrase\":\"myPassphrase\","
                                "\"mode\":\"gaia\"}"));
  test_window_->flow()->flow_handler_->HandlePassphraseEntry(&value);
  EXPECT_EQ("myPassphrase", service_->passphrase_);
}

TEST_F(SyncSetupWizardTest, PassphraseMigration) {
  SKIP_TEST_ON_MACOSX();
  wizard_->Step(SyncSetupWizard::PASSPHRASE_MIGRATION);
  ListValue value;
  value.Append(new StringValue("{\"option\":\"explicit\","
                               "\"passphrase\":\"myPassphrase\"}"));
  test_window_->flow()->flow_handler_->HandleFirstPassphrase(&value);
  EXPECT_EQ("myPassphrase", service_->passphrase_);

  ListValue value2;
  value2.Append(new StringValue("{\"option\":\"nothanks\","
                                "\"passphrase\":\"myPassphrase\"}"));
  test_window_->flow()->flow_handler_->HandleFirstPassphrase(&value2);
  EXPECT_EQ(service_->chosen_data_types_.count(syncable::PASSWORDS), 0U);
}

TEST_F(SyncSetupWizardTest, DialogCancelled) {
  SKIP_TEST_ON_MACOSX();
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  // Simulate the user closing the dialog.
  test_window_->CloseDialog();
  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_TRUE(service_->user_cancelled_dialog_);
  EXPECT_EQ(std::string(), service_->username_);
  EXPECT_EQ(std::string(), service_->password_);

  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_TRUE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  EXPECT_FALSE(test_window_->TestAndResetWasShowHTMLDialogCalled());

  test_window_->CloseDialog();
  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_TRUE(service_->user_cancelled_dialog_);
  EXPECT_EQ(std::string(), service_->username_);
  EXPECT_EQ(std::string(), service_->password_);
}

TEST_F(SyncSetupWizardTest, InvalidTransitions) {
  SKIP_TEST_ON_MACOSX();
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_FALSE(test_window_->TestAndResetWasShowHTMLDialogCalled());

  wizard_->Step(SyncSetupWizard::DONE);
  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_FALSE(test_window_->TestAndResetWasShowHTMLDialogCalled());

  wizard_->Step(SyncSetupWizard::DONE_FIRST_TIME);
  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_FALSE(test_window_->TestAndResetWasShowHTMLDialogCalled());

  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);

  wizard_->Step(SyncSetupWizard::DONE);
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, test_window_->flow()->current_state_);
  wizard_->Step(SyncSetupWizard::DONE_FIRST_TIME);
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, test_window_->flow()->current_state_);

  wizard_->Step(SyncSetupWizard::SETUP_ABORTED_BY_PENDING_CLEAR);
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN,
            test_window_->flow()->current_state_);

  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  EXPECT_EQ(SyncSetupWizard::CONFIGURE,
            test_window_->flow()->current_state_);

  wizard_->Step(SyncSetupWizard::FATAL_ERROR);
  EXPECT_EQ(SyncSetupWizard::FATAL_ERROR, test_window_->flow()->current_state_);
}

TEST_F(SyncSetupWizardTest, FullSuccessfulRunSetsPref) {
  SKIP_TEST_ON_MACOSX();
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::SETTING_UP);
  wizard_->Step(SyncSetupWizard::DONE);
  test_window_->CloseDialog();
  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_TRUE(service_->profile()->GetPrefs()->GetBoolean(
      prefs::kSyncHasSetupCompleted));
}

TEST_F(SyncSetupWizardTest, FirstFullSuccessfulRunSetsPref) {
  SKIP_TEST_ON_MACOSX();
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::SETTING_UP);
  wizard_->Step(SyncSetupWizard::DONE_FIRST_TIME);
  test_window_->CloseDialog();
  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_TRUE(service_->profile()->GetPrefs()->GetBoolean(
      prefs::kSyncHasSetupCompleted));
}

TEST_F(SyncSetupWizardTest, AbortedByPendingClear) {
  SKIP_TEST_ON_MACOSX();
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::SETUP_ABORTED_BY_PENDING_CLEAR);
  EXPECT_EQ(SyncSetupWizard::SETUP_ABORTED_BY_PENDING_CLEAR,
            test_window_->flow()->current_state_);
  test_window_->CloseDialog();
  EXPECT_FALSE(wizard_->IsVisible());
}

TEST_F(SyncSetupWizardTest, DiscreteRunChooseDataTypes) {
  SKIP_TEST_ON_MACOSX();
  // For a discrete run, we need to have ran through setup once.
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::DONE);
  test_window_->CloseDialog();
  EXPECT_TRUE(test_window_->TestAndResetWasShowHTMLDialogCalled());

  wizard_->Step(SyncSetupWizard::CONFIGURE);
  EXPECT_TRUE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_EQ(SyncSetupWizard::DONE, test_window_->flow()->end_state_);

  wizard_->Step(SyncSetupWizard::DONE);
  test_window_->CloseDialog();
  EXPECT_FALSE(wizard_->IsVisible());
}

TEST_F(SyncSetupWizardTest, DiscreteRunChooseDataTypesAbortedByPendingClear) {
  SKIP_TEST_ON_MACOSX();
  // For a discrete run, we need to have ran through setup once.
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::DONE);
  test_window_->CloseDialog();
  EXPECT_TRUE(test_window_->TestAndResetWasShowHTMLDialogCalled());

  wizard_->Step(SyncSetupWizard::CONFIGURE);
  EXPECT_TRUE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_EQ(SyncSetupWizard::DONE, test_window_->flow()->end_state_);
   wizard_->Step(SyncSetupWizard::SETUP_ABORTED_BY_PENDING_CLEAR);
  EXPECT_EQ(SyncSetupWizard::SETUP_ABORTED_BY_PENDING_CLEAR,
            test_window_->flow()->current_state_);

  test_window_->CloseDialog();
  EXPECT_FALSE(wizard_->IsVisible());
}

TEST_F(SyncSetupWizardTest, DiscreteRunGaiaLogin) {
  SKIP_TEST_ON_MACOSX();
  DictionaryValue dialog_args;
  // For a discrete run, we need to have ran through setup once.
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::SETTING_UP);
  wizard_->Step(SyncSetupWizard::DONE);
  test_window_->CloseDialog();
  EXPECT_TRUE(test_window_->TestAndResetWasShowHTMLDialogCalled());

  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  EXPECT_EQ(SyncSetupWizard::GAIA_SUCCESS, test_window_->flow()->end_state_);

  AuthError invalid_gaia(AuthError::INVALID_GAIA_CREDENTIALS);
  service_->set_auth_state(kTestUser, invalid_gaia);
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  EXPECT_TRUE(wizard_->IsVisible());
  SyncSetupFlow::GetArgsForGaiaLogin(service_, &dialog_args);
  EXPECT_EQ(5U, dialog_args.size());
  std::string iframe_to_show;
  dialog_args.GetString("iframeToShow", &iframe_to_show);
  EXPECT_EQ("login", iframe_to_show);
  std::string actual_user;
  dialog_args.GetString("user", &actual_user);
  EXPECT_EQ(kTestUser, actual_user);
  int error = -1;
  dialog_args.GetInteger("error", &error);
  EXPECT_EQ(static_cast<int>(AuthError::INVALID_GAIA_CREDENTIALS), error);
  service_->set_auth_state(kTestUser, AuthError::None());

  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  EXPECT_TRUE(test_window_->TestAndResetWasShowHTMLDialogCalled());
}

#undef SKIP_TEST_ON_MACOSX
