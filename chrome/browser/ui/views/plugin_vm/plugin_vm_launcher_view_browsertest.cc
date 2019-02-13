// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/plugin_vm/plugin_vm_launcher_view.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/window/dialog_client_view.h"

class PluginVmLauncherViewBrowserTest : public DialogBrowserTest {
 public:
  PluginVmLauncherViewBrowserTest() {}

  void SetUp() override { DialogBrowserTest::SetUp(); }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    view_ = new PluginVmLauncherView(browser()->profile());
    views::DialogDelegate::CreateDialogWidget(view_, nullptr, nullptr);
  }

  bool HasAcceptButton() {
    return view_->GetDialogClientView()->ok_button() != nullptr;
  }

  bool HasCancelButton() {
    return view_->GetDialogClientView()->cancel_button() != nullptr;
  }

  void CheckSetupIsInProgress() {
    EXPECT_TRUE(HasCancelButton());
    EXPECT_FALSE(HasAcceptButton());
    EXPECT_EQ(view_->GetWindowTitle(),
              l10n_util::GetStringUTF16(
                  IDS_PLUGIN_VM_LAUNCHER_ENVIRONMENT_SETTING_TITLE));
  }

  void CheckSetupFailed() {
    EXPECT_TRUE(HasAcceptButton());
    EXPECT_TRUE(HasCancelButton());
    EXPECT_EQ(view_->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK),
              l10n_util::GetStringUTF16(IDS_PLUGIN_VM_LAUNCHER_RETRY_BUTTON));
    EXPECT_EQ(view_->GetWindowTitle(),
              l10n_util::GetStringUTF16(IDS_PLUGIN_VM_LAUNCHER_ERROR_TITLE));
  }

  void CheckSetupIsCompleted() {
    EXPECT_TRUE(HasAcceptButton());
    EXPECT_FALSE(HasCancelButton());
    EXPECT_EQ(view_->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK),
              l10n_util::GetStringUTF16(IDS_PLUGIN_VM_LAUNCHER_LAUNCH_BUTTON));
    EXPECT_EQ(view_->GetWindowTitle(),
              l10n_util::GetStringUTF16(IDS_PLUGIN_VM_LAUNCHER_FINISHED_TITLE));
  }

 protected:
  PluginVmLauncherView* view_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PluginVmLauncherViewBrowserTest);
};

// Test the dialog is actually can be launched.
IN_PROC_BROWSER_TEST_F(PluginVmLauncherViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PluginVmLauncherViewBrowserTest, SetupCompleted) {
  // TODO(https://crbug.com/904852): Add a proper end-to-end test that
  // checks that file specified by PluginVmImage user policy is being
  // downloaded and unzipped to the specified location.

  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  CheckSetupIsInProgress();

  view_->GetPluginVmImageManagerForTesting()->OnDownloadCompleted(
      base::FilePath());

  CheckSetupIsInProgress();

  view_->GetPluginVmImageManagerForTesting()->OnUnzipped(true /* success */);

  CheckSetupIsCompleted();

  view_->GetDialogClientView()->AcceptWindow();

  EXPECT_TRUE(view_->GetWidget()->IsClosed());
}

IN_PROC_BROWSER_TEST_F(PluginVmLauncherViewBrowserTest,
                       RetryAfterDownloadFailed) {
  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  CheckSetupIsInProgress();

  view_->GetPluginVmImageManagerForTesting()->OnDownloadFailed();

  CheckSetupFailed();

  // Retry button clicked to retry the download.
  view_->GetDialogClientView()->AcceptWindow();

  CheckSetupIsInProgress();
}

IN_PROC_BROWSER_TEST_F(PluginVmLauncherViewBrowserTest,
                       RetryAfterUnzippingFailed) {
  ShowUi("default");
  EXPECT_NE(nullptr, view_);

  CheckSetupIsInProgress();

  view_->GetPluginVmImageManagerForTesting()->OnDownloadCompleted(
      base::FilePath());

  CheckSetupIsInProgress();

  view_->GetPluginVmImageManagerForTesting()->OnUnzipped(false /* success */);

  CheckSetupFailed();

  // Retry button clicked to retry the download.
  view_->GetDialogClientView()->AcceptWindow();

  CheckSetupIsInProgress();
}
