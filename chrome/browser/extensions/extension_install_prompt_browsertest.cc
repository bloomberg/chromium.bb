// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_prompt.h"

#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"

namespace {

scoped_refptr<extensions::Extension> BuildTestExtension() {
  return extensions::ExtensionBuilder()
      .SetManifest(extensions::DictionaryBuilder()
                       .Set("name", "foo")
                       .Set("version", "1.0"))
      .Build();
}

// ExtensionInstallPrompt::ShowDialogCallback which proceeds without showing the
// prompt.
void TestShowDialogCallback(
    ExtensionInstallPromptShowParams* params,
    ExtensionInstallPrompt::Delegate* delegate,
    scoped_refptr<ExtensionInstallPrompt::Prompt> prompt) {
  delegate->InstallUIProceed();
}

class TestExtensionInstallPromptDelegate
    : public ExtensionInstallPrompt::Delegate {
 public:
  explicit TestExtensionInstallPromptDelegate(const base::Closure& quit_closure)
      : quit_closure_(quit_closure), proceeded_(false), aborted_(false) {}

  ~TestExtensionInstallPromptDelegate() override {
  }

  bool DidProceed() {
    return proceeded_;
  }

  bool DidAbort() {
    return aborted_;
  }

 private:
  void InstallUIProceed() override {
    proceeded_ = true;
    quit_closure_.Run();
  }

  void InstallUIAbort(bool user_initiated) override {
    aborted_ = true;
    quit_closure_.Run();
  }

  base::Closure quit_closure_;
  bool proceeded_;
  bool aborted_;

  DISALLOW_COPY_AND_ASSIGN(TestExtensionInstallPromptDelegate);
};

}  // namespace

typedef InProcessBrowserTest ExtensionInstallPromptBrowserTest;

// Test that ExtensionInstallPrompt aborts the install if the web contents which
// were passed to the ExtensionInstallPrompt constructor get destroyed.
// CrxInstaller takes in ExtensionInstallPrompt in the constructor and does a
// bunch of asynchronous processing prior to confirming the install. A user may
// close the current tab while this processing is taking place.
IN_PROC_BROWSER_TEST_F(ExtensionInstallPromptBrowserTest,
                       TrackParentWebContentsDestruction) {
  AddBlankTabAndShow(browser());
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  content::WebContents* web_contents = tab_strip_model->GetActiveWebContents();
  int web_contents_index = tab_strip_model->GetIndexOfWebContents(web_contents);
  scoped_refptr<extensions::Extension> extension(BuildTestExtension());

  ExtensionInstallPrompt prompt(web_contents);
  tab_strip_model->CloseWebContentsAt(web_contents_index,
                                      TabStripModel::CLOSE_NONE);
  content::RunAllPendingInMessageLoop();

  base::RunLoop run_loop;
  TestExtensionInstallPromptDelegate delegate(run_loop.QuitClosure());
  prompt.ConfirmInstall(&delegate, extension.get(),
                        base::Bind(&TestShowDialogCallback));
  run_loop.Run();
  EXPECT_TRUE(delegate.DidAbort());
}

// Test that ExtensionInstallPrompt aborts the install if the gfx::NativeWindow
// which is passed to the ExtensionInstallPrompt constructor is destroyed.
IN_PROC_BROWSER_TEST_F(ExtensionInstallPromptBrowserTest,
                       TrackParentWindowDestruction) {
  // Create a second browser to prevent the app from exiting when the browser is
  // closed.
  CreateBrowser(browser()->profile());

  scoped_refptr<extensions::Extension> extension(BuildTestExtension());

  ExtensionInstallPrompt prompt(browser()->profile(),
                                browser()->window()->GetNativeWindow());
  browser()->window()->Close();
  content::RunAllPendingInMessageLoop();

  base::RunLoop run_loop;
  TestExtensionInstallPromptDelegate delegate(run_loop.QuitClosure());
  prompt.ConfirmInstall(&delegate, extension.get(),
                        base::Bind(&TestShowDialogCallback));
  run_loop.Run();
  EXPECT_TRUE(delegate.DidAbort());
}

// Test that ExtensionInstallPrompt shows the dialog normally if no parent
// web contents or parent gfx::NativeWindow is passed to the
// ExtensionInstallPrompt constructor.
IN_PROC_BROWSER_TEST_F(ExtensionInstallPromptBrowserTest, NoParent) {
  scoped_refptr<extensions::Extension> extension(BuildTestExtension());

  ExtensionInstallPrompt prompt(browser()->profile(), NULL);
  base::RunLoop run_loop;
  TestExtensionInstallPromptDelegate delegate(run_loop.QuitClosure());
  prompt.ConfirmInstall(&delegate, extension.get(),
                        base::Bind(&TestShowDialogCallback));
  run_loop.Run();

  // TestShowDialogCallback() should have signaled the install to proceed.
  EXPECT_TRUE(delegate.DidProceed());
}
