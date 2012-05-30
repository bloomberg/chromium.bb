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
#include "chrome/common/extensions/extension_switch_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

class SkBitmap;

namespace {

// Observer waits for exactly one download to finish.

class MockInstallUI : public ExtensionInstallUI {
 public:
  explicit MockInstallUI(Profile* profile) :
      ExtensionInstallUI(profile),
      did_succeed_(false),
      confirmation_requested_(false) {}

  bool did_succeed() const { return did_succeed_; }
  bool confirmation_requested() const { return confirmation_requested_; }
  const string16& error() const { return error_; }

  // Overriding some of the ExtensionInstallUI API.
  void ConfirmInstall(Delegate* delegate,
                      const extensions::Extension* extension) {
    confirmation_requested_ = true;
    delegate->InstallUIProceed();
  }
  void OnInstallSuccess(const extensions::Extension* extension,
                        SkBitmap* icon) {
    did_succeed_ = true;
    MessageLoopForUI::current()->Quit();
  }
  void OnInstallFailure(const string16& error) {
    error_ = error;
    MessageLoopForUI::current()->Quit();
  }

 private:
  bool did_succeed_;
  bool confirmation_requested_;
  string16 error_;
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
        WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
            browser()->profile(),
            id,
            scoped_ptr<base::DictionaryValue>(parsed_manifest)));

    scoped_refptr<CrxInstaller> installer(
        CrxInstaller::Create(service,
                             mock_install_ui, /* ownership transferred */
                             approval.get()   /* keep ownership */));
    installer->set_allow_silent_install(true);
    installer->set_is_gallery_install(true);
    installer->InstallCrx(PackExtension(ext_path));
    ui_test_utils::RunMessageLoop();

    EXPECT_TRUE(mock_install_ui->did_succeed());
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
  if (!extensions::switch_utils::IsOffStoreInstallEnabled())
    return;

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

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, AllowOffStore) {
#if defined(USE_AURA)
  // Off-store install cannot yet be disabled on Aura.
#else
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnableOffStoreExtensionInstall, "0");

  ExtensionService* service = browser()->profile()->GetExtensionService();
  const bool kTestData[] = {false, true};

  for (size_t i = 0; i < arraysize(kTestData); ++i) {
    MockInstallUI* mock_ui = new MockInstallUI(browser()->profile());
    scoped_refptr<CrxInstaller> crx_installer(
        CrxInstaller::Create(service, mock_ui));
    crx_installer->set_allow_off_store_install(kTestData[i]);

    crx_installer->InstallCrx(test_data_dir_.AppendASCII("good.crx"));
    EXPECT_EQ(kTestData[i], WaitForExtensionInstall()) << kTestData[i];
    EXPECT_EQ(kTestData[i], mock_ui->did_succeed());
    EXPECT_EQ(kTestData[i], mock_ui->confirmation_requested()) << kTestData[i];
    if (kTestData[i]) {
      EXPECT_EQ(string16(), mock_ui->error()) << kTestData[i];
    } else {
      EXPECT_EQ(l10n_util::GetStringUTF16(
          IDS_EXTENSION_INSTALL_DISALLOWED_ON_SITE),
          mock_ui->error()) << kTestData[i];
    }
  }
#endif
}
