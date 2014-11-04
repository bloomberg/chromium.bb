// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
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

class TestExtensionUninstallDialogDelegate
    : public extensions::ExtensionUninstallDialog::Delegate {
 public:
  explicit TestExtensionUninstallDialogDelegate(
      const base::Closure& quit_closure)
      : quit_closure_(quit_closure), canceled_(false) {}

  ~TestExtensionUninstallDialogDelegate() override {}

  bool canceled() { return canceled_; }

 private:
  void ExtensionUninstallAccepted() override { quit_closure_.Run(); }

  void ExtensionUninstallCanceled() override {
    canceled_ = true;
    quit_closure_.Run();
  }

  base::Closure quit_closure_;
  bool canceled_;

  DISALLOW_COPY_AND_ASSIGN(TestExtensionUninstallDialogDelegate);
};

}  // namespace

typedef InProcessBrowserTest ExtensionUninstallDialogViewBrowserTest;

// Test that ExtensionUninstallDialog cancels the uninstall if the aura::Window
// which is passed to ExtensionUninstallDialog::Create() is destroyed.
IN_PROC_BROWSER_TEST_F(ExtensionUninstallDialogViewBrowserTest,
                       TrackParentWindowDestruction) {
  // Create a second browser to prevent the app from exiting when the browser is
  // closed.
  CreateBrowser(browser()->profile());

  scoped_refptr<extensions::Extension> extension(BuildTestExtension());

  base::RunLoop run_loop;
  TestExtensionUninstallDialogDelegate delegate(run_loop.QuitClosure());
  scoped_ptr<extensions::ExtensionUninstallDialog> dialog(
      extensions::ExtensionUninstallDialog::Create(
          browser()->profile(), browser()->window()->GetNativeWindow(),
          &delegate));
  browser()->window()->Close();
  content::RunAllPendingInMessageLoop();

  dialog->ConfirmUninstall(extension.get());
  run_loop.Run();
  EXPECT_TRUE(delegate.canceled());
}
