// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_install_dialog_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_icon_manager.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_prompt_experiment.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/browser/ui/webui/extensions/extension_settings_handler.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/test_util.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

// A simple delegate implementation that counts the number of times
// |InstallUIProceed| and |InstallUIAbort| are called.
class MockExtensionInstallPromptDelegate
    : public ExtensionInstallPrompt::Delegate {
 public:
  MockExtensionInstallPromptDelegate()
      : proceed_count_(0),
        abort_count_(0) {}

  // ExtensionInstallPrompt::Delegate overrides.
  virtual void InstallUIProceed() OVERRIDE;
  virtual void InstallUIAbort(bool user_initiated) OVERRIDE;

  int proceed_count() { return proceed_count_; }
  int abort_count() { return abort_count_; }

 protected:
  int proceed_count_;
  int abort_count_;
};

void MockExtensionInstallPromptDelegate::InstallUIProceed() {
  ++proceed_count_;
}

void MockExtensionInstallPromptDelegate::InstallUIAbort(bool user_initiated) {
  ++abort_count_;
}

// This lets us construct the parent for the prompt we're constructing in our
// tests.
class MockExtensionInstallPrompt : public ExtensionInstallPrompt {
 public:
  explicit MockExtensionInstallPrompt(content::WebContents* web_contents)
      : ExtensionInstallPrompt(web_contents), prompt_(NULL) {}
  virtual ~MockExtensionInstallPrompt() {}
  void set_prompt(ExtensionInstallPrompt::Prompt* prompt) {
    prompt_ = prompt;
  }
  ExtensionInstallPrompt::Prompt* get_prompt() {
    return prompt_;
  }

 private:
  ExtensionInstallPrompt::Prompt* prompt_;
};

class ExtensionInstallDialogViewTestBase : public ExtensionBrowserTest {
 protected:
  explicit ExtensionInstallDialogViewTestBase(
      ExtensionInstallPrompt::PromptType prompt_type);
  virtual ~ExtensionInstallDialogViewTestBase() {}

  virtual void SetUpOnMainThread() OVERRIDE;

  ExtensionInstallPrompt::Prompt* prompt() { return prompt_.get(); }
  content::WebContents* web_contents() { return web_contents_; }
  MockExtensionInstallPromptDelegate* delegate() { return &delegate_; }

  void SetPromptPermissions(std::vector<base::string16> permissions);
  void SetPromptDetails(std::vector<base::string16> details);
  void SetPromptRetainedFiles(std::vector<base::FilePath> files);

 private:
  const extensions::Extension* extension_;
  MockExtensionInstallPrompt* install_prompt_;
  scoped_refptr<ExtensionInstallPrompt::Prompt> prompt_;
  content::WebContents* web_contents_;
  MockExtensionInstallPromptDelegate delegate_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallDialogViewTestBase);
};

ExtensionInstallDialogViewTestBase::ExtensionInstallDialogViewTestBase(
    ExtensionInstallPrompt::PromptType prompt_type)
    : extension_(NULL),
      install_prompt_(NULL),
      prompt_(new ExtensionInstallPrompt::Prompt(prompt_type)),
      web_contents_(NULL) {
}

void ExtensionInstallDialogViewTestBase::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();

  extension_ = ExtensionBrowserTest::LoadExtension(test_data_dir_.AppendASCII(
      "install_prompt/permissions_scrollbar_regression"));

  web_contents_ = browser()->tab_strip_model()->GetWebContentsAt(0);

  install_prompt_ = new MockExtensionInstallPrompt(web_contents_);
  install_prompt_->set_prompt(prompt_.get());
  prompt_->set_experiment(ExtensionInstallPromptExperiment::ControlGroup());
  prompt_->set_extension(extension_);

  scoped_ptr<ExtensionIconManager> icon_manager(new ExtensionIconManager());
  const SkBitmap icon_bitmap = icon_manager->GetIcon(extension_->id());
  gfx::Image icon = gfx::Image::CreateFrom1xBitmap(icon_bitmap);
  prompt_->set_icon(icon);

  this->SetPromptPermissions(std::vector<base::string16>());
  this->SetPromptDetails(std::vector<base::string16>());
  this->SetPromptRetainedFiles(std::vector<base::FilePath>());
}

void ExtensionInstallDialogViewTestBase::SetPromptPermissions(
    std::vector<base::string16> permissions) {
  prompt_->SetPermissions(permissions,
                          ExtensionInstallPrompt::REGULAR_PERMISSIONS);
}

void ExtensionInstallDialogViewTestBase::SetPromptDetails(
    std::vector<base::string16> details) {
  prompt_->SetPermissionsDetails(details,
                                 ExtensionInstallPrompt::REGULAR_PERMISSIONS);
}

void ExtensionInstallDialogViewTestBase::SetPromptRetainedFiles(
    std::vector<base::FilePath> files) {
  prompt_->set_retained_files(files);
}

class ScrollbarTest : public ExtensionInstallDialogViewTestBase {
 protected:
  ScrollbarTest();
  virtual ~ScrollbarTest() {}

  bool IsScrollbarVisible();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScrollbarTest);
};

ScrollbarTest::ScrollbarTest()
    : ExtensionInstallDialogViewTestBase(
          ExtensionInstallPrompt::PERMISSIONS_PROMPT) {
}

bool ScrollbarTest::IsScrollbarVisible() {
  ExtensionInstallPrompt::ShowParams show_params(web_contents());
  ExtensionInstallDialogView* dialog = new ExtensionInstallDialogView(
      show_params.navigator, delegate(), prompt());

  // Create the modal view around the install dialog view.
  views::Widget* modal =
      CreateBrowserModalDialogViews(dialog, show_params.parent_window);
  modal->Show();
  content::RunAllBlockingPoolTasksUntilIdle();

  // Check if the vertical scrollbar is visible.
  return dialog->scroll_view()->vertical_scroll_bar()->visible();
}

// Tests that a scrollbar _is_ shown for an excessively long extension
// install prompt.
IN_PROC_BROWSER_TEST_F(ScrollbarTest, LongPromptScrollbar) {
  base::string16 permission_string(base::ASCIIToUTF16("Test"));
  std::vector<base::string16> permissions;
  std::vector<base::string16> details;
  for (int i = 0; i < 20; i++) {
    permissions.push_back(permission_string);
    details.push_back(base::string16());
  }
  this->SetPromptPermissions(permissions);
  this->SetPromptDetails(details);
  ASSERT_TRUE(IsScrollbarVisible()) << "Scrollbar is not visible";
}

// Tests that a scrollbar isn't shown for this regression case.
// See crbug.com/385570 for details.
IN_PROC_BROWSER_TEST_F(ScrollbarTest, ScrollbarRegression) {
  base::string16 permission_string(base::ASCIIToUTF16(
      "Read and modify your data on *.facebook.com"));
  std::vector<base::string16> permissions;
  permissions.push_back(permission_string);
  this->SetPromptPermissions(permissions);
  std::vector<base::string16> details;
  details.push_back(base::string16());
  this->SetPromptDetails(details);
  ASSERT_FALSE(IsScrollbarVisible()) << "Scrollbar is visible";
}

class ExtensionInstallDialogViewTest
    : public ExtensionInstallDialogViewTestBase {
 protected:
  ExtensionInstallDialogViewTest()
      : ExtensionInstallDialogViewTestBase(
            ExtensionInstallPrompt::INSTALL_PROMPT) {}
  virtual ~ExtensionInstallDialogViewTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallDialogViewTest);
};

// Verifies that the delegate is notified when the user selects to accept or
// cancel the install.
IN_PROC_BROWSER_TEST_F(ExtensionInstallDialogViewTest, NotifyDelegate) {
  {
    // The user confirms the install.
    MockExtensionInstallPromptDelegate delegate;
    scoped_ptr<ExtensionInstallDialogView> dialog(
        new ExtensionInstallDialogView(web_contents(), &delegate, prompt()));
    views::DialogDelegateView* delegate_view = dialog.get();

    delegate_view->Accept();
    delegate_view->OnClosed();
    dialog.reset();

    EXPECT_EQ(0, delegate.abort_count());
    EXPECT_EQ(1, delegate.proceed_count());
  }

  {
    // The user cancels the install.
    MockExtensionInstallPromptDelegate delegate;
    scoped_ptr<ExtensionInstallDialogView> dialog(
        new ExtensionInstallDialogView(web_contents(), &delegate, prompt()));
    views::DialogDelegateView* delegate_view = dialog.get();

    delegate_view->Cancel();
    delegate_view->OnClosed();
    dialog.reset();

    EXPECT_EQ(1, delegate.abort_count());
    EXPECT_EQ(0, delegate.proceed_count());
  }

  {
    // Corner case: Dialog is closed without the user explicitly choosing to
    // proceed or cancel.
    MockExtensionInstallPromptDelegate delegate;
    scoped_ptr<ExtensionInstallDialogView> dialog(
        new ExtensionInstallDialogView(web_contents(), &delegate, prompt()));
    dialog.reset();

    EXPECT_EQ(1, delegate.abort_count());
    EXPECT_EQ(0, delegate.proceed_count());
  }
}
