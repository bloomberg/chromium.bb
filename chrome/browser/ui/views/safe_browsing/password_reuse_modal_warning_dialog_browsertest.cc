// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/password_reuse_modal_warning_dialog.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/password_protection/password_protection_service.h"
#include "ui/views/window/dialog_client_view.h"

namespace safe_browsing {

class PasswordReuseModalWarningTest : public DialogBrowserTest {
 public:
  PasswordReuseModalWarningTest()
      : dialog_(nullptr),
        latest_user_action_(PasswordProtectionService::MAX_ACTION) {}

  ~PasswordReuseModalWarningTest() override {}

  // DialogBrowserTest:
  void ShowDialog(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    dialog_ = new PasswordReuseModalWarningDialog(
        web_contents, nullptr,
        base::BindOnce(&PasswordReuseModalWarningTest::DialogCallback,
                       base::Unretained(this)));
    constrained_window::CreateBrowserModalDialogViews(
        dialog_, web_contents->GetTopLevelNativeWindow())
        ->Show();
  }

  void DialogCallback(PasswordProtectionService::WarningAction action) {
    latest_user_action_ = action;
  }

 protected:
  PasswordReuseModalWarningDialog* dialog_;
  PasswordProtectionService::WarningAction latest_user_action_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PasswordReuseModalWarningTest);
};

IN_PROC_BROWSER_TEST_F(PasswordReuseModalWarningTest, InvokeDialog_default) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(PasswordReuseModalWarningTest, InvokeDialog_softer) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      safe_browsing::kGoogleBrandedPhishingWarning,
      {{"softer_warning", "true"}});
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(PasswordReuseModalWarningTest, TestBasicDialogBehavior) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Simulating a click on ui::DIALOG_BUTTON_OK button results in a
  // CHANGE_PASSWORD action.
  ShowDialog(std::string());
  dialog_->GetDialogClientView()->AcceptWindow();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PasswordProtectionService::CHANGE_PASSWORD, latest_user_action_);

  // Simulating a click on ui::DIALOG_BUTTON_CANCEL button results in an
  // IGNORE_WARNING action.
  ShowDialog(std::string());
  dialog_->GetDialogClientView()->CancelWindow();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(PasswordProtectionService::IGNORE_WARNING, latest_user_action_);
}

// TODO(jialiul): Add true end-to-end tests when this dialog is wired into
// password protection service.

}  // namespace safe_browsing
