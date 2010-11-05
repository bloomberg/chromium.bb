// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/test/ui_test_utils.h"

namespace {

class MockInstallUI : public ExtensionInstallUI {
 public:
  explicit MockInstallUI(Profile* profile) :
      ExtensionInstallUI(profile), confirmation_requested_(false) {}

  // Did the Delegate request confirmation?
  bool confirmation_requested() { return confirmation_requested_; }

  // Overriding some of the ExtensionInstallUI API.
  void ConfirmInstall(Delegate* delegate, const Extension* extension) {
    confirmation_requested_ = true;
    delegate->InstallUIProceed();
  }
  void OnInstallSuccess(const Extension* extension) {
    MessageLoopForUI::current()->Quit();
  }
  void OnInstallFailure(const std::string& error) {
    ADD_FAILURE() << "insall failed";
    MessageLoopForUI::current()->Quit();
  }

 private:
  bool confirmation_requested_;
};

}  // namespace

class ExtensionCrxInstallerTest : public ExtensionBrowserTest {
 public:
  // Installs a crx from |crx_relpath| (a path relative to the extension test
  // data dir) with expected id |id|. Returns whether a confirmation prompt
  // happened or not.
  bool DidWhitelistInstallPrompt(const std::string& crx_relpath,
                                 const std::string& id) {
    ExtensionsService* service = browser()->profile()->GetExtensionsService();
    MockInstallUI* mock_install_ui = new MockInstallUI(browser()->profile());

    scoped_refptr<CrxInstaller> installer(
        new CrxInstaller(service->install_directory(),
                         service,
                         mock_install_ui /* ownership transferred */));

    installer->set_allow_silent_install(true);
    CrxInstaller::SetWhitelistedInstallId(id);

    FilePath crx_path = test_data_dir_.AppendASCII(crx_relpath);
    installer->InstallCrx(crx_path);
    ui_test_utils::RunMessageLoop();

    return mock_install_ui->confirmation_requested();
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, Whitelisting) {
  // A regular extension should give no prompt.
  EXPECT_FALSE(DidWhitelistInstallPrompt("good.crx",
                                         "ldnnhddmnhbkjipkidpdiheffobcpfmf"));
#if !defined(OS_CHROMEOS)
  // An extension with NPAPI should give a prompt.
  EXPECT_TRUE(DidWhitelistInstallPrompt("uitest/plugins.crx",
                                        "hdgllgikmikobbofgnabhfimcfoopgnd"));
#endif // !defined(OS_CHROMEOS
}
