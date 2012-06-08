// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_test_observer.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
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

class MockInstallPrompt : public ExtensionInstallPrompt {
 public:
  explicit MockInstallPrompt(Profile* profile) :
      ExtensionInstallPrompt(profile),
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
    MockInstallPrompt* mock_install_prompt =
        new MockInstallPrompt(browser()->profile());
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
                             mock_install_prompt, /* ownership transferred */
                             approval.get()       /* keep ownership */));
    installer->set_allow_silent_install(true);
    installer->set_is_gallery_install(true);
    installer->InstallCrx(PackExtension(ext_path));
    ui_test_utils::RunMessageLoop();

    EXPECT_TRUE(mock_install_prompt->did_succeed());
    return mock_install_prompt->confirmation_requested();
  }
};

#if defined(OS_CHROMEOS)
#define MAYBE_Whitelisting DISABLED_Whitelisting
#else
#define MAYBE_Whitelisting Whitelisting
#endif
IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, MAYBE_Whitelisting) {
  std::string id = "hdgllgikmikobbofgnabhfimcfoopgnd";
  ExtensionService* service = browser()->profile()->GetExtensionService();

  // Even whitelisted extensions with NPAPI should not prompt.
  EXPECT_FALSE(DidWhitelistInstallPrompt("uitest/plugins", id));
  EXPECT_TRUE(service->GetExtensionById(id, false));
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

// This test is disabled on Windows. See bug http://crbug.com/130951
#if defined(OS_WIN)
#define MAYBE_PackAndInstallExtension DISABLED_PackAndInstallExtension
#else
#define MAYBE_PackAndInstallExtension PackAndInstallExtension
#endif
IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       MAYBE_PackAndInstallExtension) {
  if (!extensions::switch_utils::IsEasyOffStoreInstallEnabled())
    return;

  const int kNumDownloadsExpected = 1;
  const bool kExpectFileSelectDialog = false;

  LOG(ERROR) << "PackAndInstallExtension: Packing extension";
  FilePath crx_path = PackExtension(
      test_data_dir_.AppendASCII("common/background_page"));
  ASSERT_FALSE(crx_path.empty());
  std::string crx_path_string(crx_path.value().begin(), crx_path.value().end());
  GURL url = GURL(std::string("file:///").append(crx_path_string));

  MockInstallPrompt* mock_prompt = new MockInstallPrompt(browser()->profile());
  download_crx_util::SetMockInstallPromptForTesting(mock_prompt);

  LOG(ERROR) << "PackAndInstallExtension: Getting download manager";
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(browser()->profile());

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
  EXPECT_TRUE(mock_prompt->confirmation_requested());
  LOG(ERROR) << "PackAndInstallExtension: Extension install confirmed";
}

// Off-store install cannot yet be disabled on Aura.
#if defined(USE_AURA)
#define MAYBE_AllowOffStore DISABLED_AllowOffStore
#else
#define MAYBE_AllowOffStore AllowOffStore
#endif
IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, MAYBE_AllowOffStore) {
  ExtensionService* service = browser()->profile()->GetExtensionService();
  const bool kTestData[] = {false, true};

  for (size_t i = 0; i < arraysize(kTestData); ++i) {
    MockInstallPrompt* mock_prompt =
        new MockInstallPrompt(browser()->profile());
    scoped_refptr<CrxInstaller> crx_installer(
        CrxInstaller::Create(service, mock_prompt));
    crx_installer->set_install_cause(
        extension_misc::INSTALL_CAUSE_USER_DOWNLOAD);
    crx_installer->set_allow_off_store_install(kTestData[i]);

    crx_installer->InstallCrx(test_data_dir_.AppendASCII("good.crx"));
    EXPECT_EQ(kTestData[i], WaitForExtensionInstall()) << kTestData[i];
    EXPECT_EQ(kTestData[i], mock_prompt->did_succeed());
    EXPECT_EQ(kTestData[i], mock_prompt->confirmation_requested()) <<
        kTestData[i];
    if (kTestData[i]) {
      EXPECT_EQ(string16(), mock_prompt->error()) << kTestData[i];
    } else {
      EXPECT_EQ(l10n_util::GetStringUTF16(
          IDS_EXTENSION_INSTALL_DISALLOWED_ON_SITE),
          mock_prompt->error()) << kTestData[i];
    }
  }
}
