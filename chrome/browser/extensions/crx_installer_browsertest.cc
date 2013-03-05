// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/feature_switch.h"
#include "chrome/common/extensions/permissions/permission_set.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/test/download_test_observer.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

class SkBitmap;

namespace extensions {

namespace {

// Observer waits for exactly one download to finish.

class MockInstallPrompt : public ExtensionInstallPrompt {
 public:
  explicit MockInstallPrompt(content::WebContents* web_contents) :
      ExtensionInstallPrompt(web_contents),
      confirmation_requested_(false),
      extension_(NULL) {}

  bool did_succeed() const { return !!extension_; }
  const Extension* extension() const { return extension_; }
  bool confirmation_requested() const { return confirmation_requested_; }
  const string16& error() const { return error_; }
  void set_record_oauth2_grant(bool record) { record_oauth2_grant_ = record; }

  // Overriding some of the ExtensionInstallUI API.
  virtual void ConfirmInstall(
      Delegate* delegate,
      const Extension* extension,
      const ShowDialogCallback& show_dialog_callback) OVERRIDE {
    confirmation_requested_ = true;
    delegate->InstallUIProceed();
  }
  virtual void OnInstallSuccess(const Extension* extension,
                                SkBitmap* icon) OVERRIDE {
    extension_ = extension;
    MessageLoopForUI::current()->Quit();
  }
  virtual void OnInstallFailure(const CrxInstallerError& error) OVERRIDE {
    error_ = error.message();
    MessageLoopForUI::current()->Quit();
  }

 private:
  bool confirmation_requested_;
  string16 error_;
  const Extension* extension_;
};

MockInstallPrompt* CreateMockInstallPromptForBrowser(Browser* browser) {
  return new MockInstallPrompt(
      browser->tab_strip_model()->GetActiveWebContents());
}

}  // namespace

class ExtensionCrxInstallerTest : public ExtensionBrowserTest {
 public:
  // Installs a crx from |crx_relpath| (a path relative to the extension test
  // data dir) with expected id |id|. Returns the installer.
  scoped_refptr<CrxInstaller> InstallWithPrompt(
      const std::string& ext_relpath,
      const std::string& id,
      MockInstallPrompt* mock_install_prompt) {
    ExtensionService* service = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service();
    base::FilePath ext_path = test_data_dir_.AppendASCII(ext_relpath);

    std::string error;
    base::DictionaryValue* parsed_manifest =
        extension_file_util::LoadManifest(ext_path, &error);
    if (!parsed_manifest)
      return scoped_refptr<CrxInstaller>();

    scoped_ptr<WebstoreInstaller::Approval> approval;
    if (!id.empty()) {
      approval = WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
          browser()->profile(),
          id,
          scoped_ptr<base::DictionaryValue>(parsed_manifest));
    }

    scoped_refptr<CrxInstaller> installer(
        CrxInstaller::Create(service,
                             mock_install_prompt, /* ownership transferred */
                             approval.get()       /* keep ownership */));
    installer->set_allow_silent_install(true);
    installer->set_is_gallery_install(true);
    installer->set_bypass_blacklist_for_test(true);
    installer->InstallCrx(PackExtension(ext_path));
    content::RunMessageLoop();

    EXPECT_TRUE(mock_install_prompt->did_succeed());
    return installer;
  }

  // Installs an extension and checks that it has scopes granted IFF
  // |record_oauth2_grant| is true.
  void CheckHasEmptyScopesAfterInstall(const std::string& ext_relpath,
                                       bool record_oauth2_grant) {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalExtensionApis);

    ExtensionService* service = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service();

    MockInstallPrompt* mock_prompt =
        CreateMockInstallPromptForBrowser(browser());
    mock_prompt->set_record_oauth2_grant(record_oauth2_grant);
    scoped_refptr<CrxInstaller> installer =
        InstallWithPrompt("browsertest/scopes", std::string(), mock_prompt);

    scoped_refptr<PermissionSet> permissions =
        service->extension_prefs()->GetGrantedPermissions(
            mock_prompt->extension()->id());
    ASSERT_TRUE(permissions.get());
    EXPECT_EQ(record_oauth2_grant, installer->record_oauth2_grant_);
  }
};

#if defined(OS_CHROMEOS)
#define MAYBE_Whitelisting DISABLED_Whitelisting
#else
#define MAYBE_Whitelisting Whitelisting
#endif
IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, MAYBE_Whitelisting) {
  std::string id = "hdgllgikmikobbofgnabhfimcfoopgnd";
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();

  // Even whitelisted extensions with NPAPI should not prompt.
  MockInstallPrompt* mock_prompt =
      CreateMockInstallPromptForBrowser(browser());
  scoped_refptr<CrxInstaller> installer =
      InstallWithPrompt("uitest/plugins", id, mock_prompt);
  EXPECT_FALSE(mock_prompt->confirmation_requested());
  EXPECT_TRUE(service->GetExtensionById(id, false));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       GalleryInstallGetsExperimental) {
  // We must modify the command line temporarily in order to pack an extension
  // that requests the experimental permission.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  CommandLine old_command_line = *command_line;
  command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  base::FilePath crx_path = PackExtension(
      test_data_dir_.AppendASCII("experimental"));
  ASSERT_FALSE(crx_path.empty());

  // Now reset the command line so that we are testing specifically whether
  // installing from webstore enables experimental permissions.
  *(CommandLine::ForCurrentProcess()) = old_command_line;

  EXPECT_FALSE(InstallExtension(crx_path, 0));
  EXPECT_TRUE(InstallExtensionFromWebstore(crx_path, 1));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, PlatformAppCrx) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  EXPECT_TRUE(InstallExtension(
      test_data_dir_.AppendASCII("minimal_platform_app.crx"), 1));
}

// http://crbug.com/136397
#if defined(OS_CHROMEOS)
#define MAYBE_PackAndInstallExtension DISABLED_PackAndInstallExtension
#else
#define MAYBE_PackAndInstallExtension PackAndInstallExtension
#endif
IN_PROC_BROWSER_TEST_F(
    ExtensionCrxInstallerTest, MAYBE_PackAndInstallExtension) {
  if (!FeatureSwitch::easy_off_store_install()->IsEnabled())
    return;

  const int kNumDownloadsExpected = 1;

  LOG(ERROR) << "PackAndInstallExtension: Packing extension";
  base::FilePath crx_path = PackExtension(
      test_data_dir_.AppendASCII("common/background_page"));
  ASSERT_FALSE(crx_path.empty());
  std::string crx_path_string(crx_path.value().begin(), crx_path.value().end());
  GURL url = GURL(std::string("file:///").append(crx_path_string));

  MockInstallPrompt* mock_prompt =
      CreateMockInstallPromptForBrowser(browser());
  download_crx_util::SetMockInstallPromptForTesting(mock_prompt);

  LOG(ERROR) << "PackAndInstallExtension: Getting download manager";
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(browser()->profile());

  LOG(ERROR) << "PackAndInstallExtension: Setting observer";
  scoped_ptr<content::DownloadTestObserver> observer(
      new content::DownloadTestObserverTerminal(
          download_manager, kNumDownloadsExpected,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));
  LOG(ERROR) << "PackAndInstallExtension: Navigating to URL";
  ui_test_utils::NavigateToURLWithDisposition(browser(), url, CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  EXPECT_TRUE(WaitForCrxInstallerDone());
  LOG(ERROR) << "PackAndInstallExtension: Extension install";
  EXPECT_TRUE(mock_prompt->confirmation_requested());
  LOG(ERROR) << "PackAndInstallExtension: Extension install confirmed";
}

// Tests that scopes are only granted if |record_oauth2_grant_| on the prompt is
// true.
#if defined(OS_WIN)
#define MAYBE_GrantScopes DISABLED_GrantScopes
#else
#define MAYBE_GrantScopes GrantScopes
#endif
IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, MAYBE_GrantScopes) {
  EXPECT_NO_FATAL_FAILURE(CheckHasEmptyScopesAfterInstall("browsertest/scopes",
                                                          true));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, DoNotGrantScopes) {
  EXPECT_NO_FATAL_FAILURE(CheckHasEmptyScopesAfterInstall("browsertest/scopes",
                                                          false));
}

// Off-store install cannot yet be disabled on Aura.
#if defined(USE_AURA)
#define MAYBE_AllowOffStore DISABLED_AllowOffStore
#else
#define MAYBE_AllowOffStore AllowOffStore
#endif
// Crashy: http://crbug.com/140893
IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, DISABLED_AllowOffStore) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  const bool kTestData[] = {false, true};

  for (size_t i = 0; i < arraysize(kTestData); ++i) {
    MockInstallPrompt* mock_prompt =
        CreateMockInstallPromptForBrowser(browser());
    scoped_refptr<CrxInstaller> crx_installer(
        CrxInstaller::Create(service, mock_prompt));
    crx_installer->set_install_cause(
        extension_misc::INSTALL_CAUSE_USER_DOWNLOAD);

    if (kTestData[i]) {
      crx_installer->set_off_store_install_allow_reason(
          CrxInstaller::OffStoreInstallAllowedInTest);
    }

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

}  // namespace extensions
