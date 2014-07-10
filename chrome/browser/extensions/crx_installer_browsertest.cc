// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/fake_safe_browsing_database_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/file_util.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/switches.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"
#include "chromeos/chromeos_switches.h"
#endif

class SkBitmap;

namespace extensions {

namespace {

class MockInstallPrompt;

// This class holds information about things that happen with a
// MockInstallPrompt. We create the MockInstallPrompt but need to pass
// ownership of it to CrxInstaller, so it isn't safe to hang this data on
// MockInstallPrompt itself becuase we can't guarantee it's lifetime.
class MockPromptProxy : public base::RefCountedThreadSafe<MockPromptProxy> {
 public:
  explicit MockPromptProxy(content::WebContents* web_contents);

  bool did_succeed() const { return !extension_id_.empty(); }
  const std::string& extension_id() { return extension_id_; }
  bool confirmation_requested() const { return confirmation_requested_; }
  const base::string16& error() const { return error_; }

  // To have any effect, this should be called before CreatePrompt.
  void set_record_oauth2_grant(bool record_oauth2_grant) {
    record_oauth2_grant_.reset(new bool(record_oauth2_grant));
  }

  void set_extension_id(const std::string& id) { extension_id_ = id; }
  void set_confirmation_requested() { confirmation_requested_ = true; }
  void set_error(const base::string16& error) { error_ = error; }

  scoped_ptr<ExtensionInstallPrompt> CreatePrompt();

 private:
  friend class base::RefCountedThreadSafe<MockPromptProxy>;
  virtual ~MockPromptProxy();

  // Data used to create a prompt.
  content::WebContents* web_contents_;
  scoped_ptr<bool> record_oauth2_grant_;

  // Data reported back to us by the prompt we created.
  bool confirmation_requested_;
  std::string extension_id_;
  base::string16 error_;
};

class MockInstallPrompt : public ExtensionInstallPrompt {
 public:
  MockInstallPrompt(content::WebContents* web_contents,
                    MockPromptProxy* proxy) :
      ExtensionInstallPrompt(web_contents),
      proxy_(proxy) {}

  void set_record_oauth2_grant(bool record) { record_oauth2_grant_ = record; }

  // Overriding some of the ExtensionInstallUI API.
  virtual void ConfirmInstall(
      Delegate* delegate,
      const Extension* extension,
      const ShowDialogCallback& show_dialog_callback) OVERRIDE {
    proxy_->set_confirmation_requested();
    delegate->InstallUIProceed();
  }
  virtual void OnInstallSuccess(const Extension* extension,
                                SkBitmap* icon) OVERRIDE {
    proxy_->set_extension_id(extension->id());
    base::MessageLoopForUI::current()->Quit();
  }
  virtual void OnInstallFailure(const CrxInstallerError& error) OVERRIDE {
    proxy_->set_error(error.message());
    base::MessageLoopForUI::current()->Quit();
  }

 private:
  scoped_refptr<MockPromptProxy> proxy_;
};

MockPromptProxy::MockPromptProxy(content::WebContents* web_contents)
    : web_contents_(web_contents), confirmation_requested_(false) {
}

MockPromptProxy::~MockPromptProxy() {}

scoped_ptr<ExtensionInstallPrompt> MockPromptProxy::CreatePrompt() {
  scoped_ptr<MockInstallPrompt> prompt(
      new MockInstallPrompt(web_contents_, this));
  if (record_oauth2_grant_.get())
    prompt->set_record_oauth2_grant(*record_oauth2_grant_.get());
  return prompt.PassAs<ExtensionInstallPrompt>();
}


scoped_refptr<MockPromptProxy> CreateMockPromptProxyForBrowser(
    Browser* browser) {
  return new MockPromptProxy(
      browser->tab_strip_model()->GetActiveWebContents());
}

class ManagementPolicyMock : public extensions::ManagementPolicy::Provider {
 public:
  ManagementPolicyMock() {}

  virtual std::string GetDebugPolicyProviderName() const OVERRIDE {
    return "ManagementPolicyMock";
  }

  virtual bool UserMayLoad(const Extension* extension,
                           base::string16* error) const OVERRIDE {
    *error = base::UTF8ToUTF16("Dummy error message");
    return false;
  }
};

}  // namespace

class ExtensionCrxInstallerTest : public ExtensionBrowserTest {
 public:
  scoped_ptr<WebstoreInstaller::Approval> GetApproval(
      const char* manifest_dir,
      const std::string& id,
      bool strict_manifest_checks) {
    scoped_ptr<WebstoreInstaller::Approval> result;

    base::FilePath ext_path = test_data_dir_.AppendASCII(manifest_dir);
    std::string error;
    scoped_ptr<base::DictionaryValue> parsed_manifest(
        file_util::LoadManifest(ext_path, &error));
    if (!parsed_manifest.get() || !error.empty())
      return result.Pass();

    return WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
        browser()->profile(),
        id,
        parsed_manifest.Pass(),
        strict_manifest_checks);
  }

  void RunCrxInstaller(const WebstoreInstaller::Approval* approval,
                       scoped_ptr<ExtensionInstallPrompt> prompt,
                       const base::FilePath& crx_path) {
    ExtensionService* service = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service();
    scoped_refptr<CrxInstaller> installer(
        CrxInstaller::Create(service, prompt.Pass(), approval));
    installer->set_allow_silent_install(true);
    installer->set_is_gallery_install(true);
    installer->InstallCrx(crx_path);
    content::RunMessageLoop();
  }

  // Installs a crx from |crx_relpath| (a path relative to the extension test
  // data dir) with expected id |id|.
  void InstallWithPrompt(const char* ext_relpath,
                         const std::string& id,
                         scoped_refptr<MockPromptProxy> mock_install_prompt) {
    base::FilePath ext_path = test_data_dir_.AppendASCII(ext_relpath);

    scoped_ptr<WebstoreInstaller::Approval> approval;
    if (!id.empty())
      approval = GetApproval(ext_relpath, id, true);

    base::FilePath crx_path = PackExtension(ext_path);
    EXPECT_FALSE(crx_path.empty());
    RunCrxInstaller(approval.get(), mock_install_prompt->CreatePrompt(),
                    crx_path);

    EXPECT_TRUE(mock_install_prompt->did_succeed());
  }

  // Installs an extension and checks that it has scopes granted IFF
  // |record_oauth2_grant| is true.
  void CheckHasEmptyScopesAfterInstall(const std::string& ext_relpath,
                                       bool record_oauth2_grant) {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalExtensionApis);

    scoped_refptr<MockPromptProxy> mock_prompt =
        CreateMockPromptProxyForBrowser(browser());

    mock_prompt->set_record_oauth2_grant(record_oauth2_grant);
    InstallWithPrompt("browsertest/scopes", std::string(), mock_prompt);

    scoped_refptr<PermissionSet> permissions =
        ExtensionPrefs::Get(browser()->profile())
            ->GetGrantedPermissions(mock_prompt->extension_id());
    ASSERT_TRUE(permissions.get());
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
  scoped_refptr<MockPromptProxy> mock_prompt =
      CreateMockPromptProxyForBrowser(browser());
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

  scoped_refptr<MockPromptProxy> mock_prompt =
      CreateMockPromptProxyForBrowser(browser());
  download_crx_util::SetMockInstallPromptForTesting(
      mock_prompt->CreatePrompt());

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
    scoped_refptr<MockPromptProxy> mock_prompt =
        CreateMockPromptProxyForBrowser(browser());

    scoped_refptr<CrxInstaller> crx_installer(
        CrxInstaller::Create(service, mock_prompt->CreatePrompt()));
    crx_installer->set_install_cause(
        extension_misc::INSTALL_CAUSE_USER_DOWNLOAD);

    if (kTestData[i]) {
      crx_installer->set_off_store_install_allow_reason(
          CrxInstaller::OffStoreInstallAllowedInTest);
    }

    crx_installer->InstallCrx(test_data_dir_.AppendASCII("good.crx"));
    EXPECT_EQ(kTestData[i],
              WaitForExtensionInstall()) << kTestData[i];
    EXPECT_EQ(kTestData[i], mock_prompt->did_succeed());
    EXPECT_EQ(kTestData[i], mock_prompt->confirmation_requested()) <<
        kTestData[i];
    if (kTestData[i]) {
      EXPECT_EQ(base::string16(), mock_prompt->error()) << kTestData[i];
    } else {
      EXPECT_EQ(l10n_util::GetStringUTF16(
          IDS_EXTENSION_INSTALL_DISALLOWED_ON_SITE),
          mock_prompt->error()) << kTestData[i];
    }
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, HiDpiThemeTest) {
  base::FilePath crx_path = test_data_dir_.AppendASCII("theme_hidpi_crx");
  crx_path = crx_path.AppendASCII("theme_hidpi.crx");

  ASSERT_TRUE(InstallExtension(crx_path, 1));

  const std::string extension_id("gllekhaobjnhgeagipipnkpmmmpchacm");
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  ASSERT_TRUE(service);
  const extensions::Extension* extension =
     service->GetExtensionById(extension_id, false);
  ASSERT_TRUE(extension);
  EXPECT_EQ(extension_id, extension->id());

  UninstallExtension(extension_id);
  EXPECT_FALSE(service->GetExtensionById(extension_id, false));
}

// See http://crbug.com/315299.
#if defined(OS_WIN)
#define MAYBE_InstallDelayedUntilNextUpdate \
        DISABLED_InstallDelayedUntilNextUpdate
#else
#define MAYBE_InstallDelayedUntilNextUpdate InstallDelayedUntilNextUpdate
#endif  // defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       MAYBE_InstallDelayedUntilNextUpdate) {
  const std::string extension_id("ldnnhddmnhbkjipkidpdiheffobcpfmf");
  base::FilePath crx_path = test_data_dir_.AppendASCII("delayed_install");
  ExtensionSystem* extension_system = extensions::ExtensionSystem::Get(
      browser()->profile());
  ExtensionService* service = extension_system->extension_service();
  ASSERT_TRUE(service);

  // Install version 1 of the test extension. This extension does not have
  // a background page but does have a browser action.
  ASSERT_TRUE(InstallExtension(crx_path.AppendASCII("v1.crx"), 1));
  const extensions::Extension* extension =
     service->GetExtensionById(extension_id, false);
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension_id, extension->id());
  ASSERT_EQ("1.0", extension->version()->GetString());

  // Make test extension non-idle by opening the extension's browser action
  // popup. This should cause the installation to be delayed.
  content::WindowedNotificationObserver loading_observer(
      chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
      content::Source<Profile>(profile()));
  BrowserActionTestUtil util(browser());
  // There is only one extension, so just click the first browser action.
  ASSERT_EQ(1, util.NumberOfBrowserActions());
  util.Press(0);
  loading_observer.Wait();
  ExtensionHost* extension_host =
      content::Details<ExtensionHost>(loading_observer.details()).ptr();

  // Install version 2 of the extension and check that it is indeed delayed.
  ASSERT_TRUE(UpdateExtensionWaitForIdle(
      extension_id, crx_path.AppendASCII("v2.crx"), 0));

  ASSERT_EQ(1u, service->delayed_installs()->size());
  extension = service->GetExtensionById(extension_id, false);
  ASSERT_EQ("1.0", extension->version()->GetString());

  // Make the extension idle again by closing the popup. This should not trigger
  // the delayed install.
  content::RenderProcessHostWatcher terminated_observer(
      extension_host->render_process_host(),
      content::RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  extension_host->render_view_host()->ClosePage();
  terminated_observer.Wait();
  ASSERT_EQ(1u, service->delayed_installs()->size());

  // Install version 3 of the extension. Because the extension is idle,
  // this install should succeed.
  ASSERT_TRUE(UpdateExtensionWaitForIdle(
      extension_id, crx_path.AppendASCII("v3.crx"), 0));
  extension = service->GetExtensionById(extension_id, false);
  ASSERT_EQ("3.0", extension->version()->GetString());

  // The version 2 delayed install should be cleaned up, and finishing
  // delayed extension installation shouldn't break anything.
  ASSERT_EQ(0u, service->delayed_installs()->size());
  service->MaybeFinishDelayedInstallations();
  extension = service->GetExtensionById(extension_id, false);
  ASSERT_EQ("3.0", extension->version()->GetString());
}

#if defined(FULL_SAFE_BROWSING)
IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, Blacklist) {
  scoped_refptr<FakeSafeBrowsingDatabaseManager> blacklist_db(
      new FakeSafeBrowsingDatabaseManager(true));
  Blacklist::ScopedDatabaseManagerForTest scoped_blacklist_db(blacklist_db);

  blacklist_db->SetUnsafe("gllekhaobjnhgeagipipnkpmmmpchacm");

  base::FilePath crx_path = test_data_dir_.AppendASCII("theme_hidpi_crx")
                                          .AppendASCII("theme_hidpi.crx");
  EXPECT_FALSE(InstallExtension(crx_path, 0));
}
#endif

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, NonStrictManifestCheck) {
  scoped_refptr<MockPromptProxy> mock_prompt =
      CreateMockPromptProxyForBrowser(browser());

  // We want to simulate the case where the webstore sends a more recent
  // version of the manifest, but the downloaded .crx file is old since
  // the newly published version hasn't fully propagated to all the download
  // servers yet. So load the v2 manifest, but then install the v1 crx file.
  std::string id = "lhnaeclnpobnlbjbgogdanmhadigfnjp";
  scoped_ptr<WebstoreInstaller::Approval> approval =
      GetApproval("crx_installer/v2_no_permission_change/", id, false);

  RunCrxInstaller(approval.get(), mock_prompt->CreatePrompt(),
                  test_data_dir_.AppendASCII("crx_installer/v1.crx"));

  EXPECT_TRUE(mock_prompt->did_succeed());
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, KioskOnlyTest) {
  base::FilePath crx_path =
      test_data_dir_.AppendASCII("kiosk/kiosk_only.crx");
  EXPECT_FALSE(InstallExtension(crx_path, 0));
#if defined(OS_CHROMEOS)
  // Simulate ChromeOS kiosk mode. |scoped_user_manager| will take over
  // lifetime of |user_manager|.
  chromeos::FakeUserManager* fake_user_manager =
      new chromeos::FakeUserManager();
  fake_user_manager->AddKioskAppUser("example@example.com");
  fake_user_manager->LoginUser("example@example.com");
  chromeos::ScopedUserManagerEnabler scoped_user_manager(fake_user_manager);
  EXPECT_TRUE(InstallExtension(crx_path, 1));
#endif
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, InstallToSharedLocation) {
  base::ShadowingAtExitManager at_exit_manager;
  CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kEnableExtensionAssetsSharing);
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  ExtensionAssetsManagerChromeOS::SetSharedInstallDirForTesting(
      cache_dir.path());

  base::FilePath crx_path = test_data_dir_.AppendASCII("crx_installer/v1.crx");
  const extensions::Extension* extension = InstallExtension(
      crx_path, 1, extensions::Manifest::EXTERNAL_PREF);
  base::FilePath extension_path = extension->path();
  EXPECT_TRUE(cache_dir.path().IsParent(extension_path));
  EXPECT_TRUE(base::PathExists(extension_path));

  std::string extension_id = extension->id();
  UninstallExtension(extension_id);
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  EXPECT_FALSE(service->GetExtensionById(extension_id, false));

  // In the worst case you need to repeat this up to 3 times to make sure that
  // all pending tasks we sent from UI thread to task runner and back to UI.
  for (int i = 0; i < 3; i++) {
    // Wait for background task completion that sends replay to UI thread.
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    // Wait for UI thread task completion.
    base::RunLoop().RunUntilIdle();
  }

  EXPECT_FALSE(base::PathExists(extension_path));
}
#endif

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, DoNotSync) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
                                  browser()->profile())->extension_service();
  scoped_refptr<CrxInstaller> crx_installer(
      CrxInstaller::CreateSilent(service));
  crx_installer->set_do_not_sync(true);
  crx_installer->InstallCrx(test_data_dir_.AppendASCII("good.crx"));
  EXPECT_TRUE(WaitForCrxInstallerDone());
  ASSERT_TRUE(crx_installer->extension());

  const ExtensionPrefs* extension_prefs =
      ExtensionPrefs::Get(browser()->profile());
  EXPECT_TRUE(extension_prefs->DoNotSync(crx_installer->extension()->id()));
  EXPECT_FALSE(extensions::util::ShouldSyncApp(crx_installer->extension(),
                                               browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, ManagementPolicy) {
  ManagementPolicyMock policy;
  extensions::ExtensionSystem::Get(profile())
      ->management_policy()
      ->RegisterProvider(&policy);

  base::FilePath crx_path = test_data_dir_.AppendASCII("crx_installer/v1.crx");
  EXPECT_FALSE(InstallExtension(crx_path, 0));
}

}  // namespace extensions
