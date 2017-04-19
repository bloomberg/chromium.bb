// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"

#include "base/macros.h"
#include "chrome/browser/ui/passwords/manage_passwords_test.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"

class ManagePasswordsBubbleDialogViewTest
    : public SupportsTestDialog<ManagePasswordsTest> {
 public:
  ManagePasswordsBubbleDialogViewTest() {}
  ~ManagePasswordsBubbleDialogViewTest() override {}

  void ShowDialog(const std::string& name) override {
    if (name == "PendingPasswordBubble") {
      SetupPendingPassword();
    } else if (name == "AutomaticPasswordBubble") {
      SetupAutomaticPassword();
    } else if (name == "ManagePasswordBubble") {
      SetupManagingPasswords();
      ExecuteManagePasswordsCommand();
    } else {
      ADD_FAILURE() << "Unknown dialog type";
      return;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagePasswordsBubbleDialogViewTest);
};

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleDialogViewTest,
                       InvokeDialog_PendingPasswordBubble) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleDialogViewTest,
                       InvokeDialog_AutomaticPasswordBubble) {
  RunDialog();
}

IN_PROC_BROWSER_TEST_F(ManagePasswordsBubbleDialogViewTest,
                       InvokeDialog_ManagePasswordBubble) {
  RunDialog();
}
