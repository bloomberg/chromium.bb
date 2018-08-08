// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/screens/gaia_view.h"
#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {
namespace {
constexpr base::TimeDelta kJsConditionCheckFrequency =
    base::TimeDelta::FromMilliseconds(2000);
}  // anonymous namespace

class ConsentRecordedWaiter
    : public SyncConsentScreen::SyncConsentScreenTestDelegate {
 public:
  ConsentRecordedWaiter() = default;

  void Wait() {
    if (!consent_description_strings_.empty())
      return;

    run_loop_.Run();
  }

  // SyncConsentScreen::SyncConsentScreenTestDelegate
  void OnConsentRecordedIds(const std::vector<int>& consent_description,
                            const int consent_confirmation) override {
    consent_description_ids_ = consent_description;
    consent_confirmation_id_ = consent_confirmation;
  }

  void OnConsentRecordedStrings(
      const ::login::StringList& consent_description,
      const std::string& consent_confirmation) override {
    consent_description_strings_ = consent_description;
    consent_confirmation_string_ = consent_confirmation;

    // SyncConsentScreenHandler::SyncConsentScreenHandlerTestDelegate is
    // notified after SyncConsentScreen::SyncConsentScreenTestDelegate, so
    // this is the only place where we need to quit loop.
    run_loop_.Quit();
  }

  const std::vector<int>& get_consent_description_ids() const {
    return consent_description_ids_;
  }
  int get_consent_confirmation_id() const { return consent_confirmation_id_; }
  const ::login::StringList& get_consent_description_strings() const {
    return consent_description_strings_;
  }
  const std::string& get_consent_confirmation_string() const {
    return consent_confirmation_string_;
  }

 private:
  std::vector<int> consent_description_ids_;
  int consent_confirmation_id_;

  ::login::StringList consent_description_strings_;
  std::string consent_confirmation_string_;

  base::RunLoop run_loop_;
};

// Waits for js condition to be fulfilled.
class JsConditionWaiter {
 public:
  JsConditionWaiter(const test::JSChecker& js_checker,
                    const std::string& js_condition)
      : js_checker_(js_checker), js_condition_(js_condition) {}

  ~JsConditionWaiter() = default;

  void Wait() {
    if (IsConditionFulfilled())
      return;

    timer_.Start(FROM_HERE, kJsConditionCheckFrequency, this,
                 &JsConditionWaiter::CheckCondition);
    run_loop_.Run();
  }

 private:
  bool IsConditionFulfilled() { return js_checker_.GetBool(js_condition_); }

  void CheckCondition() {
    if (IsConditionFulfilled()) {
      run_loop_.Quit();
      timer_.Stop();
    }
  }

  test::JSChecker js_checker_;
  const std::string js_condition_;

  base::RepeatingTimer timer_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(JsConditionWaiter);
};

class SyncConsentTest : public OobeBaseTest {
 public:
  SyncConsentTest() = default;
  ~SyncConsentTest() override = default;

  void TearDownOnMainThread() override {
    // If the login display is still showing, exit gracefully.
    if (LoginDisplayHost::default_host()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&chrome::AttemptExit));
      RunUntilBrowserProcessQuits();
    }

    OobeBaseTest::TearDownOnMainThread();
  }

  void LoginToSyncConsentScreen() {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources());
    WizardController::default_controller()->SkipToLoginForTesting(
        LoginScreenContext());
    observer.Wait();
    WaitForGaiaPageEvent("ready");

    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetGaiaScreenView()
        ->ShowSigninScreenForTest(OobeBaseTest::kFakeUserEmail,
                                  OobeBaseTest::kFakeUserPassword,
                                  OobeBaseTest::kEmptyUserServices);

    JsConditionWaiter(js_checker_,
                      "Oobe.getInstance().currentScreen.id == 'sync-consent'")
        .Wait();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncConsentTest);
};

IN_PROC_BROWSER_TEST_F(SyncConsentTest, SyncConsentRecorder) {
  LoginToSyncConsentScreen();

  SyncConsentScreen* screen = static_cast<SyncConsentScreen*>(
      WizardController::default_controller()->GetScreen(
          OobeScreen::SCREEN_SYNC_CONSENT));
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  screen->SetProfileSyncDisabledByPolicyForTesting(false);
  screen->SetProfileSyncEngineInitializedForTesting(true);
  screen->OnStateChanged(nullptr);

  JsConditionWaiter(js_checker_, "!$('sync-consent-impl').hidden").Wait();
  JsExpect("!$('sync-consent-impl').$.syncConsentOverviewDialog.hidden");
  JS().Evaluate(
      "$('sync-consent-impl').$. settingsSaveAndContinueButton.click()");
  consent_recorded_waiter.Wait();
  screen->SetDelegateForTesting(nullptr);  // cleanup

  const std::vector<std::string> expected_consent_strings(
      {"You're signed in!", "Chrome sync",
       "Your bookmarks, history, passwords, and other settings will be synced "
       "to your Google Account so you can use them on all your devices.",
       "Personalize Google services",
       "Google may use your browsing history to personalize Search, ads, and "
       "other Google services. You can change this anytime at "
       "myaccount.google.com/activitycontrols/search",
       "Review sync options following setup", "Accept and continue"});

  const std::vector<int> expected_consent_ids({
      IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_SYNC_NAME,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_SYNC_DESCRIPTION,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_NAME,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_DESCRIPTION,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_REVIEW_SYNC_OPTIONS_LATER,
      IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE,
  });

  const std::string expected_consent_confirmation_string =
      "Accept and continue";
  const int expected_consent_confirmation_id =
      IDS_LOGIN_SYNC_CONSENT_SCREEN_ACCEPT_AND_CONTINUE;

  EXPECT_EQ(expected_consent_strings,
            consent_recorded_waiter.get_consent_description_strings());
  EXPECT_EQ(expected_consent_confirmation_string,
            consent_recorded_waiter.get_consent_confirmation_string());
  EXPECT_EQ(expected_consent_ids,
            consent_recorded_waiter.get_consent_description_ids());
  EXPECT_EQ(expected_consent_confirmation_id,
            consent_recorded_waiter.get_consent_confirmation_id());
}

// Check that policy-disabled sync does not trigger SyncConsent screen.
//
// We need to check that "disabled by policy" disables SyncConsent screen
// independently from sync engine statis. So we run test twice, both for "sync
// engine not yet initialized" and "sync engine initialized" cases. Therefore
// we use WithParamInterface<bool> here.
class SyncConsenPolicyDisabledTest : public SyncConsentTest,
                                     public testing::WithParamInterface<bool> {
};

IN_PROC_BROWSER_TEST_P(SyncConsenPolicyDisabledTest,
                       SyncConsentPolicyDisabled) {
  LoginToSyncConsentScreen();

  SyncConsentScreen* screen = static_cast<SyncConsentScreen*>(
      WizardController::default_controller()->GetScreen(
          OobeScreen::SCREEN_SYNC_CONSENT));
  ConsentRecordedWaiter consent_recorded_waiter;
  screen->SetDelegateForTesting(&consent_recorded_waiter);

  screen->SetProfileSyncDisabledByPolicyForTesting(true);
  screen->SetProfileSyncEngineInitializedForTesting(GetParam());
  screen->OnStateChanged(nullptr);

  // Expect to see "user image selection" or some other screen here.
  JsConditionWaiter(js_checker_,
                    "Oobe.getInstance().currentScreen.id != 'sync-consent'")
      .Wait();
}

INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        SyncConsenPolicyDisabledTest,
                        testing::Bool());

}  // namespace chromeos
