// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(timsteele): Re-enable ASAP.  http://crbug.com/19002
#if 0
#ifdef CHROME_PERSONALIZATION

#include "testing/gtest/include/gtest/gtest.h"

#include "base/json_writer.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/sync/personalization.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/views/sync/sync_setup_flow.h"
#include "chrome/browser/views/sync/sync_setup_wizard.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/testing_profile.h"
#include "chrome/test/test_browser_window.h"

static const char* kTestUser = "chrome.p13n.test@gmail.com";
static const char* kTestPassword = "g00gl3g00gl3";

// A PSS subtype to inject.
class ProfileSyncServiceForWizardTest : public ProfileSyncService {
 public:
  explicit ProfileSyncServiceForWizardTest(Profile* profile)
    : ProfileSyncService(profile), user_accepted_merge_and_sync_(false),
      user_cancelled_dialog_(false) {
  }

  virtual ~ProfileSyncServiceForWizardTest() { }

  virtual void OnUserSubmittedAuth(const std::string& username,
                                   const std::string& password) {
    username_ = username;
    password_ = password;
  }
  virtual void OnUserAcceptedMergeAndSync() {
    user_accepted_merge_and_sync_ = true;
  }
  virtual void OnUserCancelledDialog() {
    user_cancelled_dialog_ = true;
  }

  virtual string16 GetAuthenticatedUsername() const {
    return UTF8ToWide(username_);
  }

  void set_auth_state(const std::string& last_email, AuthErrorState state) {
    last_attempted_user_email_ = last_email;
    last_auth_error_ = state;
  }

  void ResetTestStats() {
    username_.clear();
    password_.clear();
    user_accepted_merge_and_sync_ = false;
    user_cancelled_dialog_ = false;
  }

  std::string username_;
  std::string password_;
  bool user_accepted_merge_and_sync_;
  bool user_cancelled_dialog_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceForWizardTest);
};

class ProfilePersonalizationTestImpl : public ProfilePersonalization {
 public:
  explicit ProfilePersonalizationTestImpl(Profile* p)
      : sync_service_(new ProfileSyncServiceForWizardTest(p)) {
  }
  virtual ProfileSyncService* sync_service() { return sync_service_.get(); }
 private:
  scoped_ptr<ProfileSyncService> sync_service_;
};

class TestingProfileWithPersonalization : public TestingProfile {
 public:
  TestingProfileWithPersonalization() {
    personalization_.reset(new ProfilePersonalizationTestImpl(this));
  }

  virtual ProfilePersonalization* GetProfilePersonalization() {
    return personalization_.get();
  }
 private:
  scoped_ptr<ProfilePersonalization> personalization_;
};

class TestBrowserWindowForWizardTest : public TestBrowserWindow {
 public:
  explicit TestBrowserWindowForWizardTest(Browser* browser)
      : TestBrowserWindow(browser), flow_(NULL),
        was_show_html_dialog_called_(false) {
  }

  // We intercept this call to hijack the flow created and then use it to
  // drive the wizard as if we were the HTML page.
  virtual void ShowHTMLDialog(HtmlDialogUIDelegate* delegate,
      gfx::NativeWindow parent_window) {
    flow_ = static_cast<SyncSetupFlow*>(delegate);
    was_show_html_dialog_called_ = true;
  }

  bool TestAndResetWasShowHTMLDialogCalled() {
    bool ret = was_show_html_dialog_called_;
    was_show_html_dialog_called_ = false;
    return ret;
  }

  SyncSetupFlow* flow_;
 private:
  bool was_show_html_dialog_called_;
};

class SyncSetupWizardTest : public BrowserWithTestWindowTest {
 public:
  SyncSetupWizardTest() : test_window_(NULL), wizard_(NULL) { }
  virtual ~SyncSetupWizardTest() { }
  virtual void SetUp() {
    set_profile(new TestingProfileWithPersonalization());
    profile()->CreateBookmarkModel(false);
    // Wait for the bookmarks model to load.
    profile()->BlockUntilBookmarkModelLoaded();
    profile()->GetPrefs()->RegisterBooleanPref(prefs::kSyncHasSetupCompleted,
                                              false);
    set_browser(new Browser(Browser::TYPE_NORMAL, profile()));
    test_window_ = new TestBrowserWindowForWizardTest(browser());
    set_window(test_window_);
    browser()->set_window(window());
    BrowserList::SetLastActive(browser());
    service_ = static_cast<ProfileSyncServiceForWizardTest*>(
        profile()->GetProfilePersonalization()->sync_service());
    wizard_.reset(new SyncSetupWizard(service_));
  }

  virtual void TearDown() {
    test_window_ = NULL;
    service_ = NULL;
    wizard_.reset();
  }

  TestBrowserWindowForWizardTest* test_window_;
  scoped_ptr<SyncSetupWizard> wizard_;
  ProfileSyncServiceForWizardTest* service_;
};

TEST_F(SyncSetupWizardTest, InitialStepLogin) {
  DictionaryValue dialog_args;
  SyncSetupFlow::GetArgsForGaiaLogin(service_, &dialog_args);
  std::string json_start_args;
  JSONWriter::Write(&dialog_args, false, &json_start_args);
  ListValue credentials;
  std::string auth = "{\"user\":\"";
  auth += std::string(kTestUser) + "\",\"pass\":\"";
  auth += std::string(kTestPassword) + "\"}";
  credentials.Append(new StringValue(auth));

  EXPECT_FALSE(wizard_->IsVisible());
  EXPECT_FALSE(test_window_->flow_);
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);

  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_TRUE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, test_window_->flow_->current_state_);
  EXPECT_EQ(SyncSetupWizard::DONE, test_window_->flow_->end_state_);
  EXPECT_EQ(json_start_args, test_window_->flow_->dialog_start_args_);

  // Simulate the user submitting credentials.
  test_window_->flow_->flow_handler_->HandleSubmitAuth(&credentials);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, test_window_->flow_->current_state_);
  EXPECT_EQ(kTestUser, service_->username_);
  EXPECT_EQ(kTestPassword, service_->password_);
  EXPECT_FALSE(service_->user_accepted_merge_and_sync_);
  EXPECT_FALSE(service_->user_cancelled_dialog_);
  service_->ResetTestStats();

  // Simulate failed credentials.
  service_->set_auth_state(kTestUser, AUTH_ERROR_INVALID_GAIA_CREDENTIALS);
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_FALSE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, test_window_->flow_->current_state_);
  dialog_args.Clear();
  SyncSetupFlow::GetArgsForGaiaLogin(service_, &dialog_args);
  EXPECT_EQ(2, dialog_args.GetSize());
  std::string actual_user;
  dialog_args.GetString(L"user", &actual_user);
  EXPECT_EQ(kTestUser, actual_user);
  int error = -1;
  dialog_args.GetInteger(L"error", &error);
  EXPECT_EQ(static_cast<int>(AUTH_ERROR_INVALID_GAIA_CREDENTIALS), error);
  service_->set_auth_state(kTestUser, AUTH_ERROR_NONE);

  // Simulate success.
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_FALSE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_EQ(SyncSetupWizard::GAIA_SUCCESS, test_window_->flow_->current_state_);

  wizard_->Step(SyncSetupWizard::DONE);  // No merge and sync.
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_FALSE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_EQ(SyncSetupWizard::DONE, test_window_->flow_->current_state_);
}

TEST_F(SyncSetupWizardTest, InitialStepMergeAndSync) {
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_TRUE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_EQ(SyncSetupWizard::DONE, test_window_->flow_->end_state_);

  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::MERGE_AND_SYNC);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_FALSE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_EQ(SyncSetupWizard::MERGE_AND_SYNC,
            test_window_->flow_->current_state_);

  test_window_->flow_->flow_handler_->HandleSubmitMergeAndSync(NULL);
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_EQ(SyncSetupWizard::MERGE_AND_SYNC,
            test_window_->flow_->current_state_);
  EXPECT_EQ(std::string(), service_->username_);
  EXPECT_EQ(std::string(), service_->password_);
  EXPECT_TRUE(service_->user_accepted_merge_and_sync_);
  EXPECT_FALSE(service_->user_cancelled_dialog_);
  service_->ResetTestStats();
  wizard_->Step(SyncSetupWizard::DONE);  // No merge and sync.
  EXPECT_TRUE(wizard_->IsVisible());
  EXPECT_FALSE(test_window_->TestAndResetWasShowHTMLDialogCalled());
  EXPECT_EQ(SyncSetupWizard::DONE, test_window_->flow_->current_state_);
}

TEST_F(SyncSetupWizardTest, DialogCancelled) {
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  test_window_->flow_->OnDialogClosed("");
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
  test_window_->flow_->OnDialogClosed("");
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

  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  wizard_->Step(SyncSetupWizard::MERGE_AND_SYNC);
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, test_window_->flow_->current_state_);

  wizard_->Step(SyncSetupWizard::DONE);
  EXPECT_EQ(SyncSetupWizard::GAIA_LOGIN, test_window_->flow_->current_state_);

  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::MERGE_AND_SYNC);
  EXPECT_EQ(SyncSetupWizard::MERGE_AND_SYNC,
            test_window_->flow_->current_state_);

  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  EXPECT_EQ(SyncSetupWizard::MERGE_AND_SYNC,
            test_window_->flow_->current_state_);
}

TEST_F(SyncSetupWizardTest, FullSuccessfulRunSetsPref) {
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  wizard_->Step(SyncSetupWizard::MERGE_AND_SYNC);
  wizard_->Step(SyncSetupWizard::DONE);
  test_window_->flow_->OnDialogClosed("");
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
  test_window_->flow_->OnDialogClosed("");
  EXPECT_TRUE(test_window_->TestAndResetWasShowHTMLDialogCalled());

  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  EXPECT_EQ(SyncSetupWizard::GAIA_SUCCESS, test_window_->flow_->end_state_);

  service_->set_auth_state(kTestUser, AUTH_ERROR_INVALID_GAIA_CREDENTIALS);
  wizard_->Step(SyncSetupWizard::GAIA_LOGIN);
  EXPECT_TRUE(wizard_->IsVisible());
  SyncSetupFlow::GetArgsForGaiaLogin(service_, &dialog_args);
  EXPECT_EQ(2, dialog_args.GetSize());
  std::string actual_user;
  dialog_args.GetString(L"user", &actual_user);
  EXPECT_EQ(kTestUser, actual_user);
  int error = -1;
  dialog_args.GetInteger(L"error", &error);
  EXPECT_EQ(static_cast<int>(AUTH_ERROR_INVALID_GAIA_CREDENTIALS), error);
  service_->set_auth_state(kTestUser, AUTH_ERROR_NONE);

  wizard_->Step(SyncSetupWizard::GAIA_SUCCESS);
  EXPECT_TRUE(test_window_->TestAndResetWasShowHTMLDialogCalled());
}

#endif  // CHROME_PERSONALIZATION
#endif
