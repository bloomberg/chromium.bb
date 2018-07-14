// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_test_utils.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/login/screens/demo_setup_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chromeos/chromeos_switches.h"
#include "content/public/test/browser_test_utils.h"

using chromeos::test::DemoModeSetupResult;
using chromeos::test::MockDemoModeOfflineEnrollmentHelperCreator;
using chromeos::test::MockDemoModeOnlineEnrollmentHelperCreator;

namespace chromeos {

namespace {

constexpr char kIsConfirmationDialogHiddenQuery[] =
    "!document.querySelector('.cr-dialog-container') || "
    "!!document.querySelector('.cr-dialog-container').hidden";

constexpr base::TimeDelta kJsConditionCheckFrequency =
    base::TimeDelta::FromMilliseconds(200);

// How js query is executed.
enum class JSExecution { kSync, kAsync };

// Buttons available on OOBE dialogs.
enum class OobeButton { kBack, kNext, kText };

// Dialogs that are a part of Demo Mode setup screens.
enum class DemoSetupDialog { kEula, kSettings, kProgress, kError };

// Returns js id of the given |button| type.
std::string ButtonToStringId(OobeButton button) {
  switch (button) {
    case OobeButton::kBack:
      return "oobe-back-button";
    case OobeButton::kNext:
      return "oobe-next-button";
    case OobeButton::kText:
      return "oobe-text-button";
    default:
      NOTREACHED() << "This is not valid OOBE button type";
  }
}

// Returns js id of the given |dialog|.
std::string DialogToStringId(DemoSetupDialog dialog) {
  switch (dialog) {
    case DemoSetupDialog::kEula:
      return "eulaDialog";
    case DemoSetupDialog::kSettings:
      return "demoSetupSettingsDialog";
    case DemoSetupDialog::kProgress:
      return "demoSetupProgressDialog";
    case DemoSetupDialog::kError:
      return "demoSetupErrorDialog";
    default:
      NOTREACHED() << "This dialog does not belong to Demo Mode setup screens";
  }
}

// Returns query to access the content of the given OOBE |screen| or empty
// string if the |screen| is not a part of Demo Mode setup flow.
std::string ScreenToContentQuery(OobeScreen screen) {
  switch (screen) {
    case OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES:
      return "$('demo-preferences-content')";
    case OobeScreen::SCREEN_OOBE_EULA:
      return "$('oobe-eula-md')";
    case OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE:
      return "$('arc-tos-root')";
    case OobeScreen::SCREEN_OOBE_DEMO_SETUP:
      return "$('demo-setup-content')";
    default: {
      NOTREACHED() << "This OOBE screen is not a part of Demo Mode setup flow";
      return std::string();
    }
  }
}

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

}  // namespace

// Basic tests for demo mode setup flow.
class DemoSetupTest : public LoginManagerTest {
 public:
  DemoSetupTest() : LoginManagerTest(false) {}
  ~DemoSetupTest() override = default;

  // LoginTestManager:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kEnableDemoMode);
    command_line->AppendSwitch(chromeos::switches::kEnableOfflineDemoMode);
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    DisableConfirmationDialogAnimations();
  }

  bool IsScreenShown(OobeScreen screen) {
    const std::string screen_name = GetOobeScreenName(screen);
    const std::string query = base::StrCat(
        {"!!document.querySelector('#", screen_name,
         "') && !document.querySelector('#", screen_name, "').hidden"});
    return js_checker().GetBool(query);
  }

  bool IsConfirmationDialogShown() {
    return !js_checker().GetBool(kIsConfirmationDialogHiddenQuery);
  }

  bool IsDialogShown(OobeScreen screen, DemoSetupDialog dialog) {
    const std::string query =
        base::StrCat({"!", ScreenToContentQuery(screen), ".$.",
                      DialogToStringId(dialog), ".hidden"});
    return js_checker().GetBool(query);
  }

  bool IsScreenDialogElementShown(OobeScreen screen,
                                  DemoSetupDialog dialog,
                                  const std::string& element) {
    const std::string element_selector = base::StrCat(
        {ScreenToContentQuery(screen), ".$.", DialogToStringId(dialog),
         ".querySelector('", element, "')"});
    const std::string query = base::StrCat(
        {"!!", element_selector, " && !", element_selector, ".hidden"});
    return js_checker().GetBool(query);
  }

  void SetPlayStoreTermsForTesting() {
    EXPECT_TRUE(
        JSExecute("login.ArcTermsOfServiceScreen.setTosForTesting('Test "
                  "Play Store Terms of Service');"));
  }

  void InvokeDemoMode() {
    EXPECT_TRUE(JSExecute("cr.ui.Oobe.handleAccelerator('demo_mode');"));
  }

  void ClickOkOnConfirmationDialog() {
    EXPECT_TRUE(JSExecute("document.querySelector('.cr-dialog-ok').click();"));
  }

  void ClickCancelOnConfirmationDialog() {
    EXPECT_TRUE(
        JSExecute("document.querySelector('.cr-dialog-cancel').click();"));
  }

  // Simulates |button| click on a specified OOBE |screen|. Can be used for
  // screens that consists of one oobe-dialog element.
  void ClickOobeButton(OobeScreen screen,
                       OobeButton button,
                       JSExecution execution) {
    ClickOobeButtonWithId(screen, ButtonToStringId(button), execution);
  }

  // Simulates click on a button with |button_id| on specified OOBE |screen|.
  // Can be used for screens that consists of one oobe-dialog element.
  void ClickOobeButtonWithId(OobeScreen screen,
                             const std::string& button_id,
                             JSExecution execution) {
    const std::string query = base::StrCat(
        {ScreenToContentQuery(screen), ".$$('oobe-dialog').querySelector('",
         button_id, "').click();"});
    switch (execution) {
      case JSExecution::kAsync:
        JSExecuteAsync(query);
        return;
      case JSExecution::kSync:
        EXPECT_TRUE(JSExecute(query));
        return;
      default:
        NOTREACHED();
    }
  }

  // Simulates |button| click on a |dialog| of the specified OOBE |screen|.
  // Can be used for screens that consists of multiple oobe-dialog elements.
  void ClickScreenDialogButton(OobeScreen screen,
                               DemoSetupDialog dialog,
                               OobeButton button,
                               JSExecution execution) {
    const std::string query = base::StrCat(
        {ScreenToContentQuery(screen), ".$.", DialogToStringId(dialog),
         ".querySelector('", ButtonToStringId(button), "').click();"});
    switch (execution) {
      case JSExecution::kAsync:
        JSExecuteAsync(query);
        return;
      case JSExecution::kSync:
        EXPECT_TRUE(JSExecute(query));
        return;
      default:
        NOTREACHED();
    }
  }

  void SkipToDialog(DemoSetupDialog dialog) {
    InvokeDemoMode();
    ClickOkOnConfirmationDialog();
    WizardController::default_controller()->AdvanceToScreen(
        OobeScreen::SCREEN_OOBE_DEMO_SETUP);

    OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();
    EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_DEMO_SETUP));

    const std::string query = base::StrCat(
        {ScreenToContentQuery(OobeScreen::SCREEN_OOBE_DEMO_SETUP),
         ".showScreenForTesting('", DialogToStringId(dialog), "')"});
    EXPECT_TRUE(JSExecute(query));
    EXPECT_TRUE(IsDialogShown(OobeScreen::SCREEN_OOBE_DEMO_SETUP, dialog));
  }

  DemoSetupScreen* GetDemoSetupScreen() {
    return static_cast<DemoSetupScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            OobeScreen::SCREEN_OOBE_DEMO_SETUP));
  }

 private:
  void DisableConfirmationDialogAnimations() {
    EXPECT_TRUE(
        JSExecute("cr.ui.dialogs.BaseDialog.ANIMATE_STABLE_DURATION = 0;"));
  }

  bool JSExecute(const std::string& script) {
    return content::ExecuteScript(web_contents(), script);
  }

  void JSExecuteAsync(const std::string& script) {
    content::ExecuteScriptAsync(web_contents(), script);
  }

  DISALLOW_COPY_AND_ASSIGN(DemoSetupTest);
};

IN_PROC_BROWSER_TEST_F(DemoSetupTest, ShowConfirmationDialogAndProceed) {
  EXPECT_FALSE(IsConfirmationDialogShown());

  InvokeDemoMode();
  EXPECT_TRUE(IsConfirmationDialogShown());

  ClickOkOnConfirmationDialog();

  JsConditionWaiter(js_checker(), kIsConfirmationDialogHiddenQuery).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES));
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, ShowConfirmationDialogAndCancel) {
  EXPECT_FALSE(IsConfirmationDialogShown());

  InvokeDemoMode();
  EXPECT_TRUE(IsConfirmationDialogShown());

  ClickCancelOnConfirmationDialog();

  JsConditionWaiter(js_checker(), kIsConfirmationDialogHiddenQuery).Wait();
  EXPECT_FALSE(IsScreenShown(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES));
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, ProceedThroughSetupFlowSetupSuccess) {
  // Simulate successful online setup.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOnlineEnrollmentHelperCreator<DemoModeSetupResult::SUCCESS>);

  InvokeDemoMode();
  ClickOkOnConfirmationDialog();

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES));

  ClickOobeButton(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES, OobeButton::kText,
                  JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_EULA).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_EULA));

  ClickScreenDialogButton(OobeScreen::SCREEN_OOBE_EULA, DemoSetupDialog::kEula,
                          OobeButton::kText, JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE));

  SetPlayStoreTermsForTesting();
  ClickOobeButtonWithId(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE,
                        "#arc-tos-next-button", JSExecution::kSync);
  ClickOobeButtonWithId(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE,
                        "#arc-tos-accept-button", JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_DEMO_SETUP));
  EXPECT_TRUE(IsDialogShown(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                            DemoSetupDialog::kSettings));

  ClickScreenDialogButton(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                          DemoSetupDialog::kSettings, OobeButton::kNext,
                          JSExecution::kAsync);

  EXPECT_TRUE(IsDialogShown(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                            DemoSetupDialog::kProgress));

  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, ProceedThroughSetupFlowSetupError) {
  // Simulate online setup failure.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOnlineEnrollmentHelperCreator<DemoModeSetupResult::ERROR>);

  InvokeDemoMode();
  ClickOkOnConfirmationDialog();

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES));

  ClickOobeButton(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES, OobeButton::kText,
                  JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_EULA).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_EULA));

  ClickScreenDialogButton(OobeScreen::SCREEN_OOBE_EULA, DemoSetupDialog::kEula,
                          OobeButton::kText, JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE));

  SetPlayStoreTermsForTesting();
  ClickOobeButtonWithId(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE,
                        "#arc-tos-next-button", JSExecution::kSync);
  ClickOobeButtonWithId(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE,
                        "#arc-tos-accept-button", JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();
  EXPECT_TRUE(IsScreenShown(OobeScreen::SCREEN_OOBE_DEMO_SETUP));
  EXPECT_TRUE(IsDialogShown(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                            DemoSetupDialog::kSettings));

  ClickScreenDialogButton(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                          DemoSetupDialog::kSettings, OobeButton::kNext,
                          JSExecution::kAsync);
  EXPECT_TRUE(IsDialogShown(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                            DemoSetupDialog::kProgress));

  // Simulate online setup failure.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOnlineEnrollmentHelperCreator<DemoModeSetupResult::ERROR>);

  // Wait for progress dialog to be hidden.
  const std::string progress_dialog_hidden_query = base::StrCat(
      {"!!", ScreenToContentQuery(OobeScreen::SCREEN_OOBE_DEMO_SETUP), ".$.",
       DialogToStringId(DemoSetupDialog::kProgress), ".hidden"});
  JsConditionWaiter(js_checker(), progress_dialog_hidden_query).Wait();

  EXPECT_TRUE(IsDialogShown(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                            DemoSetupDialog::kError));

  ClickScreenDialogButton(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                          DemoSetupDialog::kError, OobeButton::kBack,
                          JSExecution::kAsync);
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_WELCOME).Wait();
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, BackOnSettingsScreen) {
  SkipToDialog(DemoSetupDialog::kSettings);

  ClickScreenDialogButton(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                          DemoSetupDialog::kSettings, OobeButton::kBack,
                          JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_WELCOME).Wait();
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, RetryOnErrorScreen) {
  // Simulate successful online setup after retry.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOnlineEnrollmentHelperCreator<DemoModeSetupResult::SUCCESS>);

  SkipToDialog(DemoSetupDialog::kError);

  ClickScreenDialogButton(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                          DemoSetupDialog::kError, OobeButton::kText,
                          JSExecution::kAsync);
  EXPECT_TRUE(IsDialogShown(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                            DemoSetupDialog::kProgress));

  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, ShowOnlineAndOfflineButton) {
  SkipToDialog(DemoSetupDialog::kSettings);
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();

  EXPECT_TRUE(IsScreenDialogElementShown(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                                         DemoSetupDialog::kSettings,
                                         "[name=onlineSetup]"));
  EXPECT_TRUE(IsScreenDialogElementShown(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                                         DemoSetupDialog::kSettings,
                                         "[name=offlineSetup]"));
}

class DemoSetupOfflineDisabledTest : public DemoSetupTest {
 public:
  DemoSetupOfflineDisabledTest() = default;
  ~DemoSetupOfflineDisabledTest() override = default;

  // DemoSetupTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    LoginManagerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kEnableDemoMode);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DemoSetupOfflineDisabledTest);
};

IN_PROC_BROWSER_TEST_F(DemoSetupOfflineDisabledTest, DoNotShowOfflineButton) {
  SkipToDialog(DemoSetupDialog::kSettings);
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();

  EXPECT_TRUE(IsScreenDialogElementShown(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                                         DemoSetupDialog::kSettings,
                                         "[name=onlineSetup]"));
  EXPECT_FALSE(IsScreenDialogElementShown(OobeScreen::SCREEN_OOBE_DEMO_SETUP,
                                          DemoSetupDialog::kSettings,
                                          "[name=offlineSetup]"));
}

}  // namespace chromeos
