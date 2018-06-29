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

constexpr char kDemoSetupContentQuery[] = "$('demo-setup-content')";

constexpr base::TimeDelta kJsConditionCheckFrequency =
    base::TimeDelta::FromMilliseconds(200);

// How js query is executed.
enum class JSExecution { kSync, kAsync };

// Buttons available on Demo Setup dialogs.
enum class OobeButton { kBack, kNext, kText };

// Dialogs that are a part of Demo Setup flow.
enum class DemoSetupDialog { kSettings, kProgress, kError };

// Returns js id of the given |button| type.
std::string OobeButtonToStringId(OobeButton button) {
  switch (button) {
    case OobeButton::kBack:
      return "oobe-back-button";
    case OobeButton::kNext:
      return "oobe-next-button";
    case OobeButton::kText:
      return "oobe-text-button";
    default:
      NOTREACHED();
  }
}

// Returns js id of the given |dialog|.
std::string DemoSetupDialogToStringId(DemoSetupDialog dialog) {
  switch (dialog) {
    case DemoSetupDialog::kSettings:
      return "demoSetupSettingsDialog";
    case DemoSetupDialog::kProgress:
      return "demoSetupProgressDialog";
    case DemoSetupDialog::kError:
      return "demoSetupErrorDialog";
    default:
      NOTREACHED();
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
  }

  void SetUpOnMainThread() override {
    LoginManagerTest::SetUpOnMainThread();
    DisableConfirmationDialogAnimations();
  }

  bool IsDemoSetupShown() {
    return js_checker().GetBool(
        "!!document.querySelector('#demo-setup') && "
        "!document.querySelector('#demo-setup').hidden");
  }

  bool IsConfirmationDialogShown() {
    return !js_checker().GetBool(kIsConfirmationDialogHiddenQuery);
  }

  bool IsDemoSetupDialogShown(DemoSetupDialog dialog) {
    const std::string query =
        base::StrCat({"!", kDemoSetupContentQuery, ".$.",
                      DemoSetupDialogToStringId(dialog), ".hidden"});
    return js_checker().GetBool(query);
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

  void ClickOobeButton(DemoSetupDialog dialog,
                       OobeButton button,
                       JSExecution execution) {
    const std::string query = base::StrCat(
        {kDemoSetupContentQuery, ".$.", DemoSetupDialogToStringId(dialog),
         ".querySelector('", OobeButtonToStringId(button), "').click();"});

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
    OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();
    EXPECT_TRUE(IsDemoSetupShown());

    const std::string query =
        base::StrCat({kDemoSetupContentQuery, ".showScreenForTesting('",
                      DemoSetupDialogToStringId(dialog), "')"});
    EXPECT_TRUE(JSExecute(query));
    EXPECT_TRUE(IsDemoSetupDialogShown(dialog));
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
  EXPECT_TRUE(IsDemoSetupShown());
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, ShowConfirmationDialogAndCancel) {
  EXPECT_FALSE(IsConfirmationDialogShown());

  InvokeDemoMode();
  EXPECT_TRUE(IsConfirmationDialogShown());

  ClickCancelOnConfirmationDialog();
  JsConditionWaiter(js_checker(), kIsConfirmationDialogHiddenQuery).Wait();
  EXPECT_FALSE(IsDemoSetupShown());
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, ProceedThroughSetupFlowSetupSuccess) {
  // Simulate successful online setup.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOnlineEnrollmentHelperCreator<DemoModeSetupResult::SUCCESS>);

  InvokeDemoMode();
  ClickOkOnConfirmationDialog();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();

  EXPECT_TRUE(IsDemoSetupShown());
  EXPECT_TRUE(IsDemoSetupDialogShown(DemoSetupDialog::kSettings));

  ClickOobeButton(DemoSetupDialog::kSettings, OobeButton::kNext,
                  JSExecution::kAsync);
  EXPECT_TRUE(IsDemoSetupDialogShown(DemoSetupDialog::kProgress));

  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, ProceedThroughSetupFlowSetupError) {
  // Simulate online setup failure.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOnlineEnrollmentHelperCreator<DemoModeSetupResult::ERROR>);

  InvokeDemoMode();
  ClickOkOnConfirmationDialog();
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_DEMO_SETUP).Wait();

  EXPECT_TRUE(IsDemoSetupShown());
  EXPECT_TRUE(IsDemoSetupDialogShown(DemoSetupDialog::kSettings));

  ClickOobeButton(DemoSetupDialog::kSettings, OobeButton::kNext,
                  JSExecution::kAsync);
  EXPECT_TRUE(IsDemoSetupDialogShown(DemoSetupDialog::kProgress));

  // Wait for progress dialog to be hidden.
  const std::string progress_dialog_hidden_query = base::StrCat(
      {"!!", kDemoSetupContentQuery, ".$.",
       DemoSetupDialogToStringId(DemoSetupDialog::kProgress), ".hidden"});
  JsConditionWaiter(js_checker(), progress_dialog_hidden_query).Wait();

  EXPECT_TRUE(IsDemoSetupDialogShown(DemoSetupDialog::kError));

  ClickOobeButton(DemoSetupDialog::kError, OobeButton::kBack,
                  JSExecution::kAsync);
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_WELCOME).Wait();
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, BackOnSettingsScreen) {
  SkipToDialog(DemoSetupDialog::kSettings);

  ClickOobeButton(DemoSetupDialog::kSettings, OobeButton::kBack,
                  JSExecution::kAsync);

  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_WELCOME).Wait();
}

IN_PROC_BROWSER_TEST_F(DemoSetupTest, RetryOnErrorScreen) {
  // Simulate successful online setup after retry.
  EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock(
      &MockDemoModeOnlineEnrollmentHelperCreator<DemoModeSetupResult::SUCCESS>);

  SkipToDialog(DemoSetupDialog::kError);

  ClickOobeButton(DemoSetupDialog::kError, OobeButton::kText,
                  JSExecution::kAsync);
  EXPECT_TRUE(IsDemoSetupDialogShown(DemoSetupDialog::kProgress));

  OobeScreenWaiter(OobeScreen::SCREEN_GAIA_SIGNIN).Wait();
}

}  // namespace chromeos
