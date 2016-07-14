// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/system_tray.h"
#include "ash/first_run/first_run_helper.h"
#include "ash/shell.h"
#include "chrome/browser/chromeos/first_run/first_run.h"
#include "chrome/browser/chromeos/first_run/first_run_controller.h"
#include "chrome/browser/chromeos/first_run/step_names.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"

namespace chromeos {

class FirstRunUIBrowserTest : public InProcessBrowserTest,
                              public FirstRunActor::Delegate {
 public:
  FirstRunUIBrowserTest()
      : initialized_(false),
        finalized_(false) {
  }

  // FirstRunActor::Delegate overrides.
  void OnActorInitialized() override {
    initialized_ = true;
    if (!on_initialized_callback_.is_null())
      on_initialized_callback_.Run();
    controller()->OnActorInitialized();
  }

  void OnNextButtonClicked(const std::string& step_name) override {
    controller()->OnNextButtonClicked(step_name);
  }

  void OnStepShown(const std::string& step_name) override {
    current_step_name_ = step_name;
    if (!on_step_shown_callback_.is_null())
      on_step_shown_callback_.Run();
    controller()->OnStepShown(step_name);
  }

  void OnStepHidden(const std::string& step_name) override {
    controller()->OnStepHidden(step_name);
  }

  void OnHelpButtonClicked() override { controller()->OnHelpButtonClicked(); }

  void OnActorFinalized() override {
    finalized_ = true;
    if (!on_finalized_callback_.is_null())
      on_finalized_callback_.Run();
    controller()->OnActorFinalized();
  }

  void OnActorDestroyed() override { controller()->OnActorDestroyed(); }

  void LaunchTutorial() {
    chromeos::first_run::LaunchTutorial();
    EXPECT_TRUE(controller() != NULL);
    // Replacing delegate to observe all messages coming from WebUI to
    // controller.
    controller()->actor_->set_delegate(this);
    initialized_ = controller()->actor_->IsInitialized();
  }

  void WaitForInitialization() {
    if (initialized_)
      return;
    WaitUntilCalled(&on_initialized_callback_);
    EXPECT_TRUE(initialized_);
    js().set_web_contents(controller()->web_contents_for_tests_);
  }

  void WaitForStep(const std::string& step_name) {
    if (current_step_name_ == step_name)
      return;
    WaitUntilCalled(&on_step_shown_callback_);
    EXPECT_EQ(current_step_name_, step_name);
  }

  void AdvanceStep() {
    js().Evaluate("cr.FirstRun.currentStep_.nextButton_.click()");
  }

  void WaitForFinalization() {
    if (!finalized_) {
      WaitUntilCalled(&on_finalized_callback_);
      EXPECT_TRUE(finalized_);
    }
  }

  void WaitUntilCalled(base::Closure* callback) {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    *callback = runner->QuitClosure();
    runner->Run();
    callback->Reset();
  }

  test::JSChecker& js() { return js_; }

  ash::FirstRunHelper* shell_helper() {
    return controller()->shell_helper_.get();
  }

  FirstRunController* controller() {
    return FirstRunController::GetInstanceForTest();
  }

 private:
  std::string current_step_name_;
  bool initialized_;
  bool finalized_;
  base::Closure on_initialized_callback_;
  base::Closure on_step_shown_callback_;
  base::Closure on_finalized_callback_;
  test::JSChecker js_;
};

IN_PROC_BROWSER_TEST_F(FirstRunUIBrowserTest, FirstRunFlow) {
  LaunchTutorial();
  WaitForInitialization();
  WaitForStep(first_run::kAppListStep);
  EXPECT_FALSE(shell_helper()->IsTrayBubbleOpened());
  AdvanceStep();
  WaitForStep(first_run::kTrayStep);
  EXPECT_TRUE(shell_helper()->IsTrayBubbleOpened());
  AdvanceStep();
  WaitForStep(first_run::kHelpStep);
  EXPECT_TRUE(shell_helper()->IsTrayBubbleOpened());
  AdvanceStep();
  WaitForFinalization();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(controller(), (void*)NULL);
  // shell_helper() is destructed already, thats why we call Shell directly.
  EXPECT_FALSE(ash::Shell::GetInstance()->GetPrimarySystemTray()->
      HasSystemBubble());
}

}  // namespace chromeos

