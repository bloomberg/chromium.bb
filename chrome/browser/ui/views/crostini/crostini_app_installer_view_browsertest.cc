// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/crostini/crostini_app_installer_view.h"

#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/crostini/crostini_browser_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/window/dialog_client_view.h"

class CrostiniAppInstallerViewBrowserTest : public CrostiniDialogBrowserTest {
 public:
  CrostiniAppInstallerViewBrowserTest()
      : CrostiniDialogBrowserTest(false /*register_termina*/) {}

  // CrostiniDialogBrowserTest:
  void ShowUi(const std::string& name) override {
    input_.success = true;
    input_.package_id = "gedit;1.1;amd64;";
    input_.name = "gedit";
    input_.version = "1.1";
    input_.description =
        "A really good package for doing lots of interesting things";
    input_.summary = "An editor";
    view_ = new CrostiniAppInstallerView(browser()->profile(), input_);
    views::DialogDelegate::CreateDialogWidget(view_, nullptr, nullptr)->Show();
  }

  CrostiniAppInstallerView* ActiveView() { return view_; }

  bool HasAcceptButton() {
    return ActiveView()->GetDialogClientView()->ok_button() != nullptr;
  }

  bool HasCancelButton() {
    return ActiveView()->GetDialogClientView()->cancel_button() != nullptr;
  }

 private:
  crostini::LinuxPackageInfo input_;
  CrostiniAppInstallerView* view_;
  DISALLOW_COPY_AND_ASSIGN(CrostiniAppInstallerViewBrowserTest);
};

IN_PROC_BROWSER_TEST_F(CrostiniAppInstallerViewBrowserTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CrostiniAppInstallerViewBrowserTest, AcceptDialog) {
  ShowUi("default");
  EXPECT_NE(nullptr, ActiveView());

  EXPECT_TRUE(HasAcceptButton());
  EXPECT_TRUE(HasCancelButton());

  ActiveView()->GetDialogClientView()->AcceptWindow();
  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());
  // TODO(https://crbug.com/921429): Check to see if the installation process
  // is handled appropiately.
}

IN_PROC_BROWSER_TEST_F(CrostiniAppInstallerViewBrowserTest, CancelDialog) {
  ShowUi("default");
  EXPECT_NE(nullptr, ActiveView());

  EXPECT_TRUE(HasAcceptButton());
  EXPECT_TRUE(HasCancelButton());

  ActiveView()->GetDialogClientView()->CancelWindow();
  EXPECT_TRUE(ActiveView()->GetWidget()->IsClosed());
}
