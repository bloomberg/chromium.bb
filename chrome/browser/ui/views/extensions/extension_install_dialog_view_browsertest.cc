// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_install_dialog_view.h"

#include <utility>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_icon_manager.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_prompt_test_helper.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/extensions/extension_settings_handler.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/test_util.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

using extensions::PermissionIDSet;
using extensions::PermissionMessage;
using extensions::PermissionMessages;

class ExtensionInstallDialogViewTestBase : public ExtensionBrowserTest {
 protected:
  explicit ExtensionInstallDialogViewTestBase(
      ExtensionInstallPrompt::PromptType prompt_type);
  ~ExtensionInstallDialogViewTestBase() override {}

  void SetUpOnMainThread() override;

  // Creates and returns an install prompt of |prompt_type_|, optionally setting
  // |permissions|.
  std::unique_ptr<ExtensionInstallPrompt::Prompt> CreatePrompt();
  std::unique_ptr<ExtensionInstallPrompt::Prompt> CreatePrompt(
      const PermissionMessages& permissions);

  content::WebContents* web_contents() { return web_contents_; }

 private:
  const extensions::Extension* extension_;
  ExtensionInstallPrompt::PromptType prompt_type_;
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallDialogViewTestBase);
};

ExtensionInstallDialogViewTestBase::ExtensionInstallDialogViewTestBase(
    ExtensionInstallPrompt::PromptType prompt_type)
    : extension_(NULL), prompt_type_(prompt_type), web_contents_(NULL) {}

void ExtensionInstallDialogViewTestBase::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();

  extension_ = ExtensionBrowserTest::LoadExtension(test_data_dir_.AppendASCII(
      "install_prompt/permissions_scrollbar_regression"));

  web_contents_ = browser()->tab_strip_model()->GetWebContentsAt(0);
}

std::unique_ptr<ExtensionInstallPrompt::Prompt>
ExtensionInstallDialogViewTestBase::CreatePrompt() {
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt(
      new ExtensionInstallPrompt::Prompt(prompt_type_));
  prompt->set_extension(extension_);

  std::unique_ptr<ExtensionIconManager> icon_manager(
      new ExtensionIconManager());
  prompt->set_icon(icon_manager->GetIcon(extension_->id()));

  return prompt;
}

class ScrollbarTest : public ExtensionInstallDialogViewTestBase {
 protected:
  ScrollbarTest();
  ~ScrollbarTest() override {}

  bool IsScrollbarVisible(
      std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt);

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollbarTest);
};

ScrollbarTest::ScrollbarTest()
    : ExtensionInstallDialogViewTestBase(
          ExtensionInstallPrompt::PERMISSIONS_PROMPT) {
}

bool ScrollbarTest::IsScrollbarVisible(
    std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt) {
  ExtensionInstallDialogView* dialog = new ExtensionInstallDialogView(
      profile(), web_contents(), ExtensionInstallPrompt::DoneCallback(),
      std::move(prompt));

  // Create the modal view around the install dialog view.
  views::Widget* modal = constrained_window::CreateBrowserModalDialogViews(
      dialog, web_contents()->GetTopLevelNativeWindow());
  modal->Show();
  content::RunAllBlockingPoolTasksUntilIdle();

  // Check if the vertical scrollbar is visible.
  return dialog->scroll_view()->vertical_scroll_bar()->visible();
}

// Tests that a scrollbar _is_ shown for an excessively long extension
// install prompt.
IN_PROC_BROWSER_TEST_F(ScrollbarTest, LongPromptScrollbar) {
  base::string16 permission_string(base::ASCIIToUTF16("Test"));
  PermissionMessages permissions;
  for (int i = 0; i < 20; i++) {
    permissions.push_back(PermissionMessage(permission_string,
                                            PermissionIDSet()));
  }
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt = CreatePrompt();
  prompt->SetPermissions(permissions,
                         ExtensionInstallPrompt::REGULAR_PERMISSIONS);
  ASSERT_TRUE(IsScrollbarVisible(std::move(prompt)))
      << "Scrollbar is not visible";
}

// Tests that a scrollbar isn't shown for this regression case.
// See crbug.com/385570 for details.
IN_PROC_BROWSER_TEST_F(ScrollbarTest, ScrollbarRegression) {
  base::string16 permission_string(base::ASCIIToUTF16(
      "Read and modify your data on *.facebook.com"));
  PermissionMessages permissions;
  permissions.push_back(PermissionMessage(permission_string,
                                          PermissionIDSet()));
  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt = CreatePrompt();
  prompt->SetPermissions(permissions,
                         ExtensionInstallPrompt::REGULAR_PERMISSIONS);
  ASSERT_FALSE(IsScrollbarVisible(std::move(prompt))) << "Scrollbar is visible";
}

class ExtensionInstallDialogViewTest
    : public ExtensionInstallDialogViewTestBase {
 protected:
  ExtensionInstallDialogViewTest()
      : ExtensionInstallDialogViewTestBase(
            ExtensionInstallPrompt::INSTALL_PROMPT) {}
  ~ExtensionInstallDialogViewTest() override {}

  views::DialogDelegateView* CreateAndShowPrompt(
      ExtensionInstallPromptTestHelper* helper) {
    std::unique_ptr<ExtensionInstallDialogView> dialog(
        new ExtensionInstallDialogView(profile(), web_contents(),
                                       helper->GetCallback(), CreatePrompt()));
    views::DialogDelegateView* delegate_view = dialog.get();

    views::Widget* modal_dialog = views::DialogDelegate::CreateDialogWidget(
        dialog.release(), nullptr,
        platform_util::GetViewForWindow(
            browser()->window()->GetNativeWindow()));
    modal_dialog->Show();

    return delegate_view;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallDialogViewTest);
};

// Verifies that the delegate is notified when the user selects to accept or
// cancel the install.
IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewTest, NotifyDelegate) {
  {
    // User presses install.
    ExtensionInstallPromptTestHelper helper;
    views::DialogDelegateView* delegate_view = CreateAndShowPrompt(&helper);
    delegate_view->GetDialogClientView()->AcceptWindow();
    EXPECT_EQ(ExtensionInstallPrompt::Result::ACCEPTED, helper.result());
  }
  {
    // User presses cancel.
    ExtensionInstallPromptTestHelper helper;
    views::DialogDelegateView* delegate_view = CreateAndShowPrompt(&helper);
    delegate_view->GetDialogClientView()->CancelWindow();
    EXPECT_EQ(ExtensionInstallPrompt::Result::USER_CANCELED, helper.result());
  }
  {
    // Dialog is closed without the user explicitly choosing to proceed or
    // cancel.
    ExtensionInstallPromptTestHelper helper;
    views::DialogDelegateView* delegate_view = CreateAndShowPrompt(&helper);
    delegate_view->GetWidget()->Close();
    // TODO(devlin): Should this be ABORTED?
    EXPECT_EQ(ExtensionInstallPrompt::Result::USER_CANCELED, helper.result());
  }
}
