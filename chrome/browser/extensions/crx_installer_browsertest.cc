// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/crx_installer.h"

#include <stddef.h>

#include <utility>

#include "base/at_exit.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/fake_safe_browsing_database_manager.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/web_application_info.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/install/crx_install_error.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/extension.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/file_util.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"
#include "chromeos/chromeos_switches.h"
#endif

class SkBitmap;

namespace {

const char kAppUrl[] = "http://www.google.com";
const char kAppTitle[] = "Test title";
const char kAppDescription[] = "Test description";

}  // anonymous namespace

namespace extensions {

namespace {

class MockInstallPrompt;

// This class holds information about things that happen with a
// MockInstallPrompt. We create the MockInstallPrompt but need to pass
// ownership of it to CrxInstaller, so it isn't safe to hang this data on
// MockInstallPrompt itself becuase we can't guarantee it's lifetime.
class MockPromptProxy {
 public:
  explicit MockPromptProxy(content::WebContents* web_contents);
  ~MockPromptProxy();

  bool did_succeed() const { return !extension_id_.empty(); }
  const std::string& extension_id() { return extension_id_; }
  bool confirmation_requested() const { return confirmation_requested_; }
  const base::string16& error() const { return error_; }

  void set_extension_id(const std::string& id) { extension_id_ = id; }
  void set_confirmation_requested(bool requested) {
    confirmation_requested_ = requested;
  }
  void set_error(const base::string16& error) { error_ = error; }

  std::unique_ptr<ExtensionInstallPrompt> CreatePrompt();

 private:

  // Data used to create a prompt.
  content::WebContents* web_contents_;

  // Data reported back to us by the prompt we created.
  bool confirmation_requested_;
  std::string extension_id_;
  base::string16 error_;

  std::unique_ptr<ScopedTestDialogAutoConfirm> auto_confirm;

  DISALLOW_COPY_AND_ASSIGN(MockPromptProxy);
};

SkBitmap CreateSquareBitmap(int size) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseColor(SK_ColorRED);
  return bitmap;
}

WebApplicationInfo::IconInfo CreateIconInfoWithBitmap(int size) {
  WebApplicationInfo::IconInfo icon_info;
  icon_info.width = size;
  icon_info.height = size;
  icon_info.data = CreateSquareBitmap(size);
  return icon_info;
}

WebApplicationInfo CreateWebAppInfo(const char* title,
                                    const char* description,
                                    const char* app_url,
                                    int size) {
  WebApplicationInfo web_app_info;
  web_app_info.title = base::UTF8ToUTF16(title);
  web_app_info.description = base::UTF8ToUTF16(description);
  web_app_info.app_url = GURL(app_url);
  web_app_info.scope = GURL(app_url);

  web_app_info.icons.push_back(CreateIconInfoWithBitmap(size));

  return web_app_info;
}

class MockInstallPrompt : public ExtensionInstallPrompt {
 public:
  MockInstallPrompt(content::WebContents* web_contents,
                    MockPromptProxy* proxy) :
      ExtensionInstallPrompt(web_contents),
      proxy_(proxy) {}

  // Overriding some of the ExtensionInstallUI API.
  void OnInstallSuccess(const Extension* extension, SkBitmap* icon) override {
    proxy_->set_extension_id(extension->id());
    proxy_->set_confirmation_requested(did_call_show_dialog());
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }
  void OnInstallFailure(const CrxInstallError& error) override {
    proxy_->set_error(error.message());
    proxy_->set_confirmation_requested(did_call_show_dialog());
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

 private:
  MockPromptProxy* proxy_;

  DISALLOW_COPY_AND_ASSIGN(MockInstallPrompt);
};

MockPromptProxy::MockPromptProxy(content::WebContents* web_contents)
    : web_contents_(web_contents),
      confirmation_requested_(false),
      auto_confirm(new ScopedTestDialogAutoConfirm(
          ScopedTestDialogAutoConfirm::ACCEPT)) {
}

MockPromptProxy::~MockPromptProxy() {}

std::unique_ptr<ExtensionInstallPrompt> MockPromptProxy::CreatePrompt() {
  return std::unique_ptr<MockInstallPrompt>(
      new MockInstallPrompt(web_contents_, this));
}

std::unique_ptr<MockPromptProxy> CreateMockPromptProxyForBrowser(
    Browser* browser) {
  return base::MakeUnique<MockPromptProxy>(
      browser->tab_strip_model()->GetActiveWebContents());
}

class ManagementPolicyMock : public extensions::ManagementPolicy::Provider {
 public:
  ManagementPolicyMock() {}

  std::string GetDebugPolicyProviderName() const override {
    return "ManagementPolicyMock";
  }

  bool UserMayLoad(const Extension* extension,
                   base::string16* error) const override {
    if (error)
      *error = base::UTF8ToUTF16("Dummy error message");
    return false;
  }
};

}  // namespace

class ExtensionCrxInstallerTest : public ExtensionBrowserTest {
 protected:
  std::unique_ptr<WebstoreInstaller::Approval> GetApproval(
      const char* manifest_dir,
      const std::string& id,
      bool strict_manifest_checks) {
    std::unique_ptr<WebstoreInstaller::Approval> result;

    base::ThreadRestrictions::ScopedAllowIO allow_io;
    base::FilePath ext_path = test_data_dir_.AppendASCII(manifest_dir);
    std::string error;
    std::unique_ptr<base::DictionaryValue> parsed_manifest(
        file_util::LoadManifest(ext_path, &error));
    if (!parsed_manifest.get() || !error.empty())
      return result;

    return WebstoreInstaller::Approval::CreateWithNoInstallPrompt(
        browser()->profile(), id, std::move(parsed_manifest),
        strict_manifest_checks);
  }

  void RunCrxInstaller(const WebstoreInstaller::Approval* approval,
                       std::unique_ptr<ExtensionInstallPrompt> prompt,
                       const base::FilePath& crx_path) {
    ExtensionService* service = extensions::ExtensionSystem::Get(
        browser()->profile())->extension_service();
    scoped_refptr<CrxInstaller> installer(
        CrxInstaller::Create(service, std::move(prompt), approval));
    installer->set_allow_silent_install(true);
    installer->set_is_gallery_install(true);
    installer->InstallCrx(crx_path);
    content::RunMessageLoop();
  }

  // Installs a crx from |crx_relpath| (a path relative to the extension test
  // data dir) with expected id |id|.
  void InstallWithPrompt(const char* ext_relpath,
                         const std::string& id,
                         MockPromptProxy* mock_install_prompt) {
    base::FilePath ext_path = test_data_dir_.AppendASCII(ext_relpath);

    std::unique_ptr<WebstoreInstaller::Approval> approval;
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
    std::unique_ptr<MockPromptProxy> mock_prompt =
        CreateMockPromptProxyForBrowser(browser());

    InstallWithPrompt("browsertest/scopes", std::string(), mock_prompt.get());

    std::unique_ptr<const PermissionSet> permissions =
        ExtensionPrefs::Get(browser()->profile())
            ->GetGrantedPermissions(mock_prompt->extension_id());
    ASSERT_TRUE(permissions.get());
  }

  void InstallWebAppAndVerifyNoErrors() {
    ExtensionService* service =
        extensions::ExtensionSystem::Get(browser()->profile())
            ->extension_service();
    scoped_refptr<CrxInstaller> crx_installer(
        CrxInstaller::CreateSilent(service));
    crx_installer->set_error_on_unsupported_requirements(true);
    crx_installer->InstallWebApp(
        CreateWebAppInfo(kAppTitle, kAppDescription, kAppUrl, 64));
    EXPECT_TRUE(WaitForCrxInstallerDone());
    ASSERT_TRUE(crx_installer->extension());
  }
};

class ExtensionCrxInstallerTestWithExperimentalApis
    : public ExtensionCrxInstallerTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionCrxInstallerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       ExperimentalExtensionFromGallery) {
  // Gallery-installed extensions should have their experimental permission
  // preserved, since we allow the Webstore to make that decision.
  const Extension* extension = InstallExtensionFromWebstore(
      test_data_dir_.AppendASCII("experimental.crx"), 1);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      APIPermission::kExperimental));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       ExperimentalExtensionFromOutsideGallery) {
  // Non-gallery-installed extensions should lose their experimental
  // permission if the flag isn't enabled.
  const Extension* extension = InstallExtension(
      test_data_dir_.AppendASCII("experimental.crx"), 1);
  ASSERT_TRUE(extension);
  EXPECT_FALSE(extension->permissions_data()->HasAPIPermission(
      APIPermission::kExperimental));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTestWithExperimentalApis,
                       ExperimentalExtensionFromOutsideGalleryWithFlag) {
  // Non-gallery-installed extensions should maintain their experimental
  // permission if the flag is enabled.
  const Extension* extension = InstallExtension(
      test_data_dir_.AppendASCII("experimental.crx"), 1);
  ASSERT_TRUE(extension);
  EXPECT_TRUE(extension->permissions_data()->HasAPIPermission(
      APIPermission::kExperimental));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTestWithExperimentalApis,
                       PlatformAppCrx) {
  EXPECT_TRUE(InstallExtension(
      test_data_dir_.AppendASCII("minimal_platform_app.crx"), 1));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, BlockedFileTypes) {
  const Extension* extension =
      InstallExtension(test_data_dir_.AppendASCII("blocked_file_types.crx"), 1);
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  EXPECT_TRUE(base::PathExists(extension->path().AppendASCII("test.html")));
  EXPECT_TRUE(base::PathExists(extension->path().AppendASCII("test.nexe")));
  EXPECT_FALSE(base::PathExists(extension->path().AppendASCII("test1.EXE")));
  EXPECT_FALSE(base::PathExists(extension->path().AppendASCII("test2.exe")));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, AllowedThemeFileTypes) {
  const Extension* extension = InstallExtension(
      test_data_dir_.AppendASCII("theme_with_extension.crx"), 1);
  ASSERT_TRUE(extension);
  const base::FilePath& path = extension->path();
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  EXPECT_TRUE(
      base::PathExists(path.AppendASCII("images/theme_frame_camo.PNG")));
  EXPECT_TRUE(
      base::PathExists(path.AppendASCII("images/theme_ntp_background.png")));
  EXPECT_TRUE(base::PathExists(
      path.AppendASCII("images/theme_ntp_background_norepeat.png")));
  EXPECT_TRUE(
      base::PathExists(path.AppendASCII("images/theme_toolbar_camo.png")));
  EXPECT_TRUE(base::PathExists(path.AppendASCII("images/redirect_target.GIF")));
  EXPECT_TRUE(base::PathExists(path.AppendASCII("test.image.bmp")));
  EXPECT_TRUE(
      base::PathExists(path.AppendASCII("test_image_with_no_extension")));

  EXPECT_FALSE(base::PathExists(path.AppendASCII("non_images/test.html")));
  EXPECT_FALSE(base::PathExists(path.AppendASCII("non_images/test.nexe")));
  EXPECT_FALSE(base::PathExists(path.AppendASCII("non_images/test1.EXE")));
  EXPECT_FALSE(base::PathExists(path.AppendASCII("non_images/test2.exe")));
  EXPECT_FALSE(base::PathExists(path.AppendASCII("non_images/test.txt")));
  EXPECT_FALSE(base::PathExists(path.AppendASCII("non_images/test.css")));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       PackAndInstallExtensionFromDownload) {
  std::unique_ptr<base::AutoReset<bool>> allow_offstore_install =
      download_crx_util::OverrideOffstoreInstallAllowedForTesting(true);

  const int kNumDownloadsExpected = 1;

  base::FilePath crx_path = PackExtension(
      test_data_dir_.AppendASCII("common/background_page"));
  ASSERT_FALSE(crx_path.empty());
  std::string crx_path_string(crx_path.value().begin(), crx_path.value().end());
  GURL url = GURL(std::string("file:///").append(crx_path_string));

  std::unique_ptr<MockPromptProxy> mock_prompt =
      CreateMockPromptProxyForBrowser(browser());
  download_crx_util::SetMockInstallPromptForTesting(
      mock_prompt->CreatePrompt());

  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(browser()->profile());

  std::unique_ptr<content::DownloadTestObserver> observer(
      new content::DownloadTestObserverTerminal(
          download_manager, kNumDownloadsExpected,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  EXPECT_TRUE(WaitForCrxInstallerDone());
  EXPECT_TRUE(mock_prompt->confirmation_requested());
}

// Tests that scopes are only granted if |record_oauth2_grant_| on the prompt is
// true.
#if defined(OS_WIN)
#define MAYBE_GrantScopes DISABLED_GrantScopes
#else
#define MAYBE_GrantScopes GrantScopes
#endif
IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTestWithExperimentalApis,
                       MAYBE_GrantScopes) {
  EXPECT_NO_FATAL_FAILURE(CheckHasEmptyScopesAfterInstall("browsertest/scopes",
                                                          true));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTestWithExperimentalApis,
                       DoNotGrantScopes) {
  EXPECT_NO_FATAL_FAILURE(CheckHasEmptyScopesAfterInstall("browsertest/scopes",
                                                          false));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, AllowOffStore) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  const bool kTestData[] = {false, true};

  for (size_t i = 0; i < arraysize(kTestData); ++i) {
    std::unique_ptr<MockPromptProxy> mock_prompt =
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
    // The |mock_prompt| will quit running the loop once the |crx_installer|
    // is done.
    content::RunMessageLoop();
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
  ExtensionRegistry* registry = ExtensionRegistry::Get(
      browser()->profile());
  const extensions::Extension* extension =
     registry->enabled_extensions().GetByID(extension_id);
  ASSERT_TRUE(extension);
  EXPECT_EQ(extension_id, extension->id());

  UninstallExtension(extension_id);
  EXPECT_FALSE(registry->enabled_extensions().GetByID(extension_id));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       InstallDelayedUntilNextUpdate) {
  const std::string extension_id("ldnnhddmnhbkjipkidpdiheffobcpfmf");
  base::FilePath base_path = test_data_dir_.AppendASCII("delayed_install");

  ExtensionSystem* extension_system = extensions::ExtensionSystem::Get(
      browser()->profile());
  ExtensionService* service = extension_system->extension_service();
  ASSERT_TRUE(service);
  ExtensionRegistry* registry = ExtensionRegistry::Get(
      browser()->profile());
  ASSERT_TRUE(registry);

  // Install version 1 of the test extension. This extension does not have
  // a background page but does have a browser action.
  base::FilePath v1_path = PackExtension(base_path.AppendASCII("v1"));
  ASSERT_FALSE(v1_path.empty());
  ASSERT_TRUE(InstallExtension(v1_path, 1));
  const extensions::Extension* extension =
     registry->enabled_extensions().GetByID(extension_id);
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension_id, extension->id());
  ASSERT_EQ("1.0", extension->version()->GetString());

  // Make test extension non-idle by opening the extension's options page.
  ExtensionTabUtil::OpenOptionsPage(extension, browser());
  WaitForExtensionNotIdle(extension_id);

  // Install version 2 of the extension and check that it is indeed delayed.
  base::FilePath v2_path = PackExtension(base_path.AppendASCII("v2"));
  ASSERT_FALSE(v2_path.empty());
  ASSERT_TRUE(UpdateExtensionWaitForIdle(extension_id, v2_path, 0));

  ASSERT_EQ(1u, service->delayed_installs()->size());
  extension = registry->enabled_extensions().GetByID(extension_id);
  ASSERT_EQ("1.0", extension->version()->GetString());

  // Make the extension idle again by navigating away from the options page.
  // This should not trigger the delayed install.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  WaitForExtensionIdle(extension_id);
  ASSERT_EQ(1u, service->delayed_installs()->size());
  extension = registry->enabled_extensions().GetByID(extension_id);
  ASSERT_EQ("1.0", extension->version()->GetString());

  // Install version 3 of the extension. Because the extension is idle,
  // this install should succeed.
  base::FilePath v3_path = PackExtension(base_path.AppendASCII("v3"));
  ASSERT_FALSE(v3_path.empty());
  ASSERT_TRUE(UpdateExtensionWaitForIdle(extension_id, v3_path, 0));
  extension = registry->enabled_extensions().GetByID(extension_id);
  ASSERT_EQ("3.0", extension->version()->GetString());

  // The version 2 delayed install should be cleaned up, and finishing
  // delayed extension installation shouldn't break anything.
  ASSERT_EQ(0u, service->delayed_installs()->size());
  service->MaybeFinishDelayedInstallations();
  extension = registry->enabled_extensions().GetByID(extension_id);
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
  std::unique_ptr<MockPromptProxy> mock_prompt =
      CreateMockPromptProxyForBrowser(browser());

  // We want to simulate the case where the webstore sends a more recent
  // version of the manifest, but the downloaded .crx file is old since
  // the newly published version hasn't fully propagated to all the download
  // servers yet. So load the v2 manifest, but then install the v1 crx file.
  std::string id = "lhnaeclnpobnlbjbgogdanmhadigfnjp";
  std::unique_ptr<WebstoreInstaller::Approval> approval =
      GetApproval("crx_installer/v2_no_permission_change/", id, false);

  RunCrxInstaller(approval.get(), mock_prompt->CreatePrompt(),
                  test_data_dir_.AppendASCII("crx_installer/v1.crx"));

  EXPECT_TRUE(mock_prompt->did_succeed());
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, KioskOnlyTest) {
  // kiosk_only is whitelisted from non-chromeos.
  base::FilePath crx_path =
      test_data_dir_.AppendASCII("kiosk/kiosk_only.crx");
  EXPECT_FALSE(InstallExtension(crx_path, 0));
  // Simulate ChromeOS kiosk mode. |scoped_user_manager| will take over
  // lifetime of |user_manager|.
  chromeos::FakeChromeUserManager* fake_user_manager =
      new chromeos::FakeChromeUserManager();
  const AccountId account_id(AccountId::FromUserEmail("example@example.com"));
  fake_user_manager->AddKioskAppUser(account_id);
  fake_user_manager->LoginUser(account_id);
  chromeos::ScopedUserManagerEnabler scoped_user_manager(fake_user_manager);
  EXPECT_TRUE(InstallExtension(crx_path, 1));
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, InstallToSharedLocation) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kEnableExtensionAssetsSharing);
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  ExtensionAssetsManagerChromeOS::SetSharedInstallDirForTesting(
      cache_dir.GetPath());

  base::FilePath crx_path = test_data_dir_.AppendASCII("crx_installer/v1.crx");
  const extensions::Extension* extension = InstallExtension(
      crx_path, 1, extensions::Manifest::EXTERNAL_PREF);
  base::FilePath extension_path = extension->path();
  EXPECT_TRUE(cache_dir.GetPath().IsParent(extension_path));
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  EXPECT_TRUE(base::PathExists(extension_path));

  std::string extension_id = extension->id();
  UninstallExtension(extension_id);
  ExtensionRegistry* registry = ExtensionRegistry::Get(
      browser()->profile());
  EXPECT_FALSE(registry->enabled_extensions().GetByID(extension_id));

  content::RunAllTasksUntilIdle();

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
  EXPECT_FALSE(extensions::util::ShouldSync(crx_installer->extension(),
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

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, InstallWebApp) {
  InstallWebAppAndVerifyNoErrors();
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest,
                       InstallWebAppSucceedsWithBlockPolicy) {
  // Verify that the install still works when a management policy blocking
  // extension installation is in force. Bookmark apps are special-cased to skip
  // these checks (see https://crbug.com/545541).
  ManagementPolicyMock policy;
  extensions::ExtensionSystem::Get(profile())
      ->management_policy()
      ->RegisterProvider(&policy);

  InstallWebAppAndVerifyNoErrors();
}

IN_PROC_BROWSER_TEST_F(ExtensionCrxInstallerTest, UpdateWithFileAccess) {
  base::FilePath ext_source =
      test_data_dir_.AppendASCII("permissions").AppendASCII("files");
  base::FilePath crx_with_file_permission = PackExtension(ext_source);
  ASSERT_FALSE(crx_with_file_permission.empty());

  ExtensionService* service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();

  const std::string extension_id("bdkapipdccfifhdghmblnenbbncfcpid");
  {
    // Install extension.
    scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(service));
    installer->InstallCrx(crx_with_file_permission);
    EXPECT_TRUE(WaitForCrxInstallerDone());
    const Extension* extension = installer->extension();
    ASSERT_TRUE(extension);
    // IDs must match, otherwise the test doesn't make any sense.
    ASSERT_EQ(extension_id, extension->id());
    // Sanity check: File access should be disabled by default.
    EXPECT_FALSE(ExtensionPrefs::Get(profile())->AllowFileAccess(extension_id));
    EXPECT_FALSE(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS);
  }

  {
    // Uninstall and re-install the extension. Any previously granted file
    // permissions should be gone.
    ExtensionPrefs::Get(profile())->SetAllowFileAccess(extension_id, true);
    EXPECT_TRUE(ExtensionPrefs::Get(profile())->AllowFileAccess(extension_id));
    UninstallExtension(extension_id);
    EXPECT_FALSE(ExtensionPrefs::Get(profile())->AllowFileAccess(extension_id));

    scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(service));
    installer->InstallCrx(crx_with_file_permission);
    EXPECT_TRUE(WaitForCrxInstallerDone());
    const Extension* extension = installer->extension();
    ASSERT_TRUE(extension);
    ASSERT_EQ(extension_id, extension->id());
    EXPECT_FALSE(ExtensionPrefs::Get(profile())->AllowFileAccess(extension_id));
    EXPECT_FALSE(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS);
  }

  {
    // Grant file access and update the extension. File access should be kept.
    ExtensionPrefs::Get(profile())->SetAllowFileAccess(extension_id, true);
    EXPECT_TRUE(ExtensionPrefs::Get(profile())->AllowFileAccess(extension_id));
    base::FilePath crx_update_with_file_permission = PackExtension(ext_source);

    scoped_refptr<CrxInstaller> installer(CrxInstaller::CreateSilent(service));
    installer->InstallCrx(crx_update_with_file_permission);
    EXPECT_TRUE(WaitForCrxInstallerDone());
    const Extension* extension = installer->extension();
    ASSERT_TRUE(extension);
    ASSERT_EQ(extension_id, extension->id());
    EXPECT_TRUE(ExtensionPrefs::Get(profile())->AllowFileAccess(extension_id));
    EXPECT_TRUE(extension->creation_flags() & Extension::ALLOW_FILE_ACCESS);
  }
}

}  // namespace extensions
