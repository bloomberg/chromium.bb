// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_test_observer.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/test/base/ui_test_utils.h"

class SkBitmap;

namespace {

// Observer waits for exactly one download to finish.

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
  void OnInstallSuccess(const Extension* extension, SkBitmap* icon) {
    MessageLoopForUI::current()->Quit();
  }
  void OnInstallFailure(const string16& error) {
    ADD_FAILURE() << "install failed";
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
  bool DidWhitelistInstallPrompt(const std::string& ext_relpath,
                                 const std::string& id) {
    ExtensionService* service = browser()->profile()->GetExtensionService();
    MockInstallUI* mock_install_ui = new MockInstallUI(browser()->profile());
    FilePath ext_path = test_data_dir_.AppendASCII(ext_relpath);

    std::string error;
    base::DictionaryValue* parsed_manifest =
        extension_file_util::LoadManifest(ext_path, &error);
    if (!parsed_manifest)
      return false;

    scoped_ptr<WebstoreInstaller::Approval> approval(
        new WebstoreInstaller::Approval);
    approval->extension_id = id;
    approval->profile = browser()->profile();
    approval->parsed_manifest.reset(parsed_manifest);

    scoped_refptr<CrxInstaller> installer(
        CrxInstaller::Create(service,
                             mock_install_ui, /* ownership transferred */
                             approval.get()   /* keep ownership */));
    installer->set_allow_silent_install(true);
    installer->set_is_gallery_install(true);
    installer->InstallCrx(PackExtension(ext_path));
    ui_test_utils::RunMessageLoop();

    return mock_install_ui->confirmation_requested();
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, Whitelisting) {
#if !defined(OS_CHROMEOS)
  std::string id = "hdgllgikmikobbofgnabhfimcfoopgnd";
  ExtensionService* service = browser()->profile()->GetExtensionService();

  // Even whitelisted extensions with NPAPI should not prompt.
  EXPECT_FALSE(DidWhitelistInstallPrompt("uitest/plugins", id));
  EXPECT_TRUE(service->GetExtensionById(id, false));
#endif  // !defined(OS_CHROMEOS)
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       GalleryInstallGetsExperimental) {
  // We must modify the command line temporarily in order to pack an extension
  // that requests the experimental permission.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  CommandLine old_command_line = *command_line;
  command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  FilePath crx_path = PackExtension(
      test_data_dir_.AppendASCII("experimental"));
  ASSERT_FALSE(crx_path.empty());

  // Now reset the command line so that we are testing specifically whether
  // installing from webstore enables experimental permissions.
  *(CommandLine::ForCurrentProcess()) = old_command_line;

  EXPECT_FALSE(InstallExtension(crx_path, 0));
  EXPECT_TRUE(InstallExtensionFromWebstore(crx_path, 1));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, PlatformAppCrx) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  command_line->AppendSwitch(switches::kEnablePlatformApps);
  EXPECT_TRUE(InstallExtension(
      test_data_dir_.AppendASCII("minimal_platform_app.crx"), 1));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, PackAndInstallExtension) {
  const int kNumDownloadsExpected = 1;
  const bool kExpectFileSelectDialog = false;

  LOG(ERROR) << "PackAndInstallExtension: Packing extension";
  FilePath crx_path = PackExtension(
      test_data_dir_.AppendASCII("common/background_page"));
  ASSERT_FALSE(crx_path.empty());
  std::string crx_path_string(crx_path.value().begin(), crx_path.value().end());
  GURL url = GURL(std::string("file:///").append(crx_path_string));

  MockInstallUI* mock_ui = new MockInstallUI(browser()->profile());
  download_crx_util::SetMockInstallUIForTesting(mock_ui);

  LOG(ERROR) << "PackAndInstallExtension: Getting download manager";
  content::DownloadManager* download_manager =
      DownloadServiceFactory::GetForProfile(
          browser()->profile())->GetDownloadManager();

  LOG(ERROR) << "PackAndInstallExtension: Setting observer";
  scoped_ptr<DownloadTestObserver> observer(
      new DownloadTestObserverTerminal(
          download_manager, kNumDownloadsExpected, kExpectFileSelectDialog,
          DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));
  LOG(ERROR) << "PackAndInstallExtension: Navigating to URL";
  ui_test_utils::NavigateToURLWithDisposition(browser(), url, CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  EXPECT_TRUE(WaitForExtensionInstall());
  LOG(ERROR) << "PackAndInstallExtension: Extension install";
  EXPECT_TRUE(mock_ui->confirmation_requested());
  LOG(ERROR) << "PackAndInstallExtension: Extension install confirmed";
}
