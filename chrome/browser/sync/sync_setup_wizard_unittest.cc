// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/json/json_writer.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/google_service_auth_error.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/sync_setup_flow.h"
#include "chrome/browser/sync/sync_setup_wizard.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/testing_profile.h"
#include "chrome/test/test_browser_window.h"

static const char* kTestUser = "chrome.p13n.test@gmail.com";
static const char* kTestPassword = "passwd";
static const char* kTestCaptcha = "pizzamyheart";
static const char* kTestCaptchaUrl = "http://pizzamyheart/";

typedef GoogleServiceAuthError AuthError;

// A PSS subtype to inject.
class ProfileSyncServiceForWizardTest : public ProfileSyncService {
 public:
  explicit ProfileSyncServiceForWizardTest(Profile* profile)
    : ProfileSyncService(profile), user_accepted_merge_and_sync_(false),
      user_cancelled_dialog_(false) {
    RegisterPreferences();
  }

  virtual ~ProfileSyncServiceForWizardTest() { }

  virtual void OnUserSubmittedAuth(const std::string& username,
                                   const std::string& password,
                                   const std::string& captcha) {
    username_ = username;
    password_ = password;
    captcha_ = captcha;
  }
  virtual void OnUserAcceptedMergeAndSync() {
    user_accepted_merge_and_sync_ = true;
  }
  virtual void OnUserCancelledDialog() {
    user_cancelled_dialog_ = true;
  }

  virtual string16 GetAuthenticatedUsername() const {
    return UTF8ToUTF16(username_);
  }

  void set_auth_state(const std::string& last_email,
                      const AuthError& error) {
    last_attempted_user_email_ = last_email;
    last_auth_error_ = error;
  }

  void ResetTestStats() {
    username_.clear();
    password_.clear();
    captcha_.clear();
    user_accepted_merge_and_sync_ = false;
    user_cancelled_dialog_ = false;
  }

  std::string username_;
  std::string password_;
  std::string captcha_;
  bool user_accepted_merge_and_sync_;
  bool user_cancelled_dialog_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceForWizardTest);
};

class TestingProfileWithSyncService : public TestingProfile {
 public:
  TestingProfileWithSyncService() {
    sync_service_.reset(new ProfileSyncServiceForWizardTest(this));
  }

  virtual ProfileSyncService* GetProfileSyncService() {
    return sync_service_.get();
  }
 private:
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
      // which calls GetDOMMessageHandlers to take ownership.  This does not
      // exist in our test, so we perform cleanup manually.
      std::vector<DOMMessageHandler*> handlers;
      flow_->GetDOMMessageHandlers(&handlers);
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
      std::vector<DOMMessageHandler*> handlers;
      flow_->GetDOMMessageHandlers(&handlers);
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
      : ui_thread_(ChromeThread::UI, MessageLoop::current()),
        file_thread_(ChromeThread::FILE, MessageLoop::current()),
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

  ChromeThread ui_thread_;
  ChromeThread file_thread_;
  TestBrowserWindowForWizardTest* test_window_;
  scoped_ptr<SyncSetupWizard> wizard_;
  ProfileSyncServiceForWizardTest* service_;
};

TEST_F(SyncSetupWizardTest, InitialStepLogin) {
  DictionaryValue dialog_args;
  SyncSetupFlow::GetArgsForGaiaLogin(service_, &dialog_args);
  std::string json_start_args;
  base::JSONWriter::Write(&dialog_args, false, &json_start_args);
  ListValue credentials;
  std::string auth = "{\"user\":\"";
  auth += std::string(kTestUser) + "\",\"pass\":\"";
  auth += std::string(kTestPassword) + "\",\"captcha\":\"";
  auth += std::string(kTestCaptcha) + "\"}";
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
  EXPECT_FALSE(service_->user_accepted_merge_and_sync_);
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
  EXPECT_TRUE(3 == dialog_args.GetSize());
  std::string actual_user;
  dialog_args.GetString(L"user", &actual_user);
  EXPECT_EQ(kTestUser, actual_user);
  int error = -1;
  dialog_args.GetInteger(L"error", &error);
  EXPECT_EQ(static_cast<int>(AuthError::INVALID_GAIA_CREDENTIALS), error);
  service_->set_auth_state(kTestUser, AuthError::None());

  // Simulate captcha.
  AuthError captcha_error(AuthError::FromCaptchaChallenge(
      std::string(), GURL(kTestCaptchaUrl), GURL()));
  service_->set_auth_state(kTestUser, captcha_error);
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  SyncSetupFlow::GetArgsForGaiaLogin(service_, &dialog_args);
  EXPECT_TRUE(3 == dialog_args.GetSize());
  std::string captcha_url;
  dialog_args.GetString(L"captchaUrl", &captcha_url);
  EXPECT_EQ(kTestCaptchaUrl, GURL(captcha_url).spec());
  error = -1;
  dialog_args.GetInteger(L"error", &error);
  EXPECT_EQ(static_cast<int>(AuthError::CAPTCHA_REQUIRED), error);
  service_->set_auth_state(kTestUser, AuthError::None());

  // Simulate success.
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_FALSE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_EQ(SyncSetupWizard::GAIA_SUCCESS,
            test_window_->flow()->current_state_);

  wizard_->Step(SyncSetupWizard::DONE);  // No merge and sync.
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_FALSE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_EQ(SyncSetupWizard::DONE, test_window_->flow()->current_state_);
}

TEST_F(SyncSetupWizardTest, InitialStepMergeAndSync) {
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_TRUE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_EQ(SyncSetupWizard::DONE, test_window_->flow()->end_state_);

  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::MERGE_AND_SYNC);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_FALSE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_EQ(SyncSetupWizard::MERGE_AND_SYNC,
            test_window_->flow()->current_state_);

  test_window_->flow()->flow_handler_->HandleSubmitMergeAndSync(NULL);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_EQ(SyncSetupWizard::MERGE_AND_SYNC,
            test_window_->flow()->current_state_);
  EXPECT_EQ(std::string(), service_->username_);
  EXPECT_EQ(std::string(), service_->password_);
  EXPECT_TRUE(service_->user_accepted_merge_and_sync_);
  EXPECT_FALSE(service_->user_cancelled_dialog_);
  service_->ResetTestStats();
  wizard_->Step(SyncSetupWizard::DONE_FIRST_TIME);  // No merge and sync.
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_FALSE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_EQ(SyncSetupWizard::DONE_FIRST_TIME,
            test_window_->flow()->current_state_);
}

TEST_F(SyncSetupWizardTest, DialogCancelled) {
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  // Simulate the user closing the dialog.
  test_window_->CloseDialog();
  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_TRUE(service_->user_cancelled_dialog_);
  EXPECT_EQ(std::string(), service_->username_);
  EXPECT_EQ(std::string(), service_->password_);
  EXPECT_FALSE(service_->user_accepted_merge_and_sync_);

  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_TRUE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  EXPECT_FALSE(test_window_->TestAndResetWasShowHTMLDialogCalled());

  wizard_->Step(SyncSetupWizard::MERGE_AND_SYNC);
  test_window_->CloseDialog();
  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_TRUE(service_->user_cancelled_dialog_);
  EXPECT_EQ(std::string(), service_->username_);
  EXPECT_EQ(std::string(), service_->password_);
  EXPECT_FALSE(service_->user_accepted_merge_and_sync_);
}

TEST_F(SyncSetupWizardTest, InvalidTransitions) {
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
  wizard_->Step(SyncSetupWizard::MERGE_AND_SYNC);
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, test_window_->flow()->current_state_);

  wizard_->Step(SyncSetupWizard::DONE);
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, test_window_->flow()->current_state_);
  wizard_->Step(SyncSetupWizard::DONE_FIRST_TIME);
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, test_window_->flow()->current_state_);

  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::MERGE_AND_SYNC);
  EXPECT_EQ(SyncSetupWizard::MERGE_AND_SYNC,
            test_window_->flow()->current_state_);

  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  EXPECT_EQ(SyncSetupWizard::MERGE_AND_SYNC,
            test_window_->flow()->current_state_);

  wizard_->Step(SyncSetupWizard::FATAL_ERROR);
  EXPECT_EQ(SyncSetupWizard::FATAL_ERROR, test_window_->flow()->current_state_);
}

TEST_F(SyncSetupWizardTest, FullSuccessfulRunSetsPref) {
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::MERGE_AND_SYNC);
  wizard_->Step(SyncSetupWizard::DONE);
  test_window_->CloseDialog();
  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_TRUE(service_->profile()->GetPrefs()->GetBoolean(
      prefs::kSyncHasSetupCompleted));
}

TEST_F(SyncSetupWizardTest, FirstFullSuccessfulRunSetsPref) {
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::MERGE_AND_SYNC);
  wizard_->Step(SyncSetupWizard::DONE_FIRST_TIME);
  test_window_->CloseDialog();
  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_TRUE(service_->profile()->GetPrefs()->GetBoolean(
      prefs::kSyncHasSetupCompleted));
}

TEST_F(SyncSetupWizardTest, DiscreteRun) {
  DictionaryValue dialog_args;
  // For a discrete run, we need to have ran through setup once.
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::MERGE_AND_SYNC);
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
  EXPECT_TRUE(3 == dialog_args.GetSize());
  std::string actual_user;
  dialog_args.GetString(L"user", &actual_user);
  EXPECT_EQ(kTestUser, actual_user);
  int error = -1;
  dialog_args.GetInteger(L"error", &error);
  EXPECT_EQ(static_cast<int>(AuthError::INVALID_GAIA_CREDENTIALS), error);
  service_->set_auth_state(kTestUser, AuthError::None());

  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  EXPECT_TRUE(test_window_->TestAndResetWasShowHTMLDialogCalled());
}
