// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_creator.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_install_prompt_show_params.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_cache_fake.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "components/sync/model/string_ordinal.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

using extensions::Extension;
using extensions::ExtensionCreator;
using extensions::ExtensionRegistry;
using extensions::FeatureSwitch;
using extensions::Manifest;
using extensions::ScopedTestDialogAutoConfirm;

ExtensionBrowserTest::ExtensionBrowserTest()
    : loaded_(false),
      installed_(false),
#if defined(OS_CHROMEOS)
      set_chromeos_user_(true),
#endif
      // Default channel is STABLE but override with UNKNOWN so that unlaunched
      // or incomplete APIs can write tests.
      current_channel_(version_info::Channel::UNKNOWN),
      override_prompt_for_external_extensions_(
          FeatureSwitch::prompt_for_external_extensions(),
          false),
#if defined(OS_WIN)
      user_desktop_override_(base::DIR_USER_DESKTOP),
      common_desktop_override_(base::DIR_COMMON_DESKTOP),
      user_quick_launch_override_(base::DIR_USER_QUICK_LAUNCH),
      start_menu_override_(base::DIR_START_MENU),
      common_start_menu_override_(base::DIR_COMMON_START_MENU),
#endif
      profile_(NULL) {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
}

ExtensionBrowserTest::~ExtensionBrowserTest() {
}

Profile* ExtensionBrowserTest::profile() {
  if (!profile_) {
    if (browser())
      profile_ = browser()->profile();
    else
      profile_ = ProfileManager::GetActiveUserProfile();
  }
  return profile_;
}

// static
const Extension* ExtensionBrowserTest::GetExtensionByPath(
    const extensions::ExtensionSet& extensions,
    const base::FilePath& path) {
  base::FilePath extension_path = base::MakeAbsoluteFilePath(path);
  EXPECT_TRUE(!extension_path.empty());
  for (const scoped_refptr<const Extension>& extension : extensions) {
    if (extension->path() == extension_path) {
      return extension.get();
    }
  }
  return NULL;
}

void ExtensionBrowserTest::SetUp() {
  test_extension_cache_.reset(new extensions::ExtensionCacheFake());
  InProcessBrowserTest::SetUp();
}

void ExtensionBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("extensions");
  observer_.reset(new ChromeExtensionTestNotificationObserver(browser()));

  // We don't want any warning bubbles for, e.g., unpacked extensions.
  ExtensionMessageBubbleFactory::set_override_for_tests(
      ExtensionMessageBubbleFactory::OVERRIDE_DISABLED);

#if defined(OS_CHROMEOS)
  if (set_chromeos_user_) {
    // This makes sure that we create the Default profile first, with no
    // ExtensionService and then the real profile with one, as we do when
    // running on chromeos.
    command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                    "testuser@gmail.com");
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
  }
#endif
}

void ExtensionBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  observer_.reset(new ChromeExtensionTestNotificationObserver(browser()));
  if (extension_service()->updater()) {
    extension_service()->updater()->SetExtensionCacheForTesting(
        test_extension_cache_.get());
  }
}

void ExtensionBrowserTest::TearDownOnMainThread() {
  ExtensionMessageBubbleFactory::set_override_for_tests(
      ExtensionMessageBubbleFactory::NO_OVERRIDE);
  InProcessBrowserTest::TearDownOnMainThread();
}

const Extension* ExtensionBrowserTest::LoadExtension(
    const base::FilePath& path) {
  return LoadExtensionWithFlags(path, kFlagEnableFileAccess);
}

const Extension* ExtensionBrowserTest::LoadExtensionIncognito(
    const base::FilePath& path) {
  return LoadExtensionWithFlags(path,
                                kFlagEnableFileAccess | kFlagEnableIncognito);
}

const Extension* ExtensionBrowserTest::LoadExtensionWithFlags(
    const base::FilePath& path, int flags) {
  return LoadExtensionWithInstallParam(path, flags, std::string());
}

const extensions::Extension*
ExtensionBrowserTest::LoadExtensionWithInstallParam(
    const base::FilePath& path,
    int flags,
    const std::string& install_param) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      profile())->extension_service();
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  {
    observer_->Watch(extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
                     content::NotificationService::AllSources());

    scoped_refptr<extensions::UnpackedInstaller> installer(
        extensions::UnpackedInstaller::Create(service));
    installer->set_prompt_for_plugins(false);
    installer->set_require_modern_manifest_version(
        (flags & kFlagAllowOldManifestVersions) == 0);
    installer->Load(path);

    observer_->Wait();
  }

  // Find the loaded extension by its path. See crbug.com/59531 for why
  // we cannot just use last_loaded_extension_id().
  const Extension* extension =
      GetExtensionByPath(registry->enabled_extensions(), path);
  if (!extension)
    return NULL;

  if (!(flags & kFlagIgnoreManifestWarnings)) {
    const std::vector<extensions::InstallWarning>& install_warnings =
        extension->install_warnings();
    if (!install_warnings.empty()) {
      std::string install_warnings_message = base::StringPrintf(
          "Unexpected warnings when loading test extension %s:\n",
          path.AsUTF8Unsafe().c_str());

      for (std::vector<extensions::InstallWarning>::const_iterator it =
          install_warnings.begin(); it != install_warnings.end(); ++it) {
        install_warnings_message += "  " + it->message + "\n";
      }

      EXPECT_EQ(0u, extension->install_warnings().size())
          << install_warnings_message;
      return NULL;
    }
  }

  const std::string extension_id = extension->id();

  // If this is an incognito test (e.g. where the test fixture appended the
  // --incognito flag), we need to use the original profile when we wait for
  // notifications.
  Profile* original_profile = profile()->GetOriginalProfile();

  if (!install_param.empty()) {
    extensions::ExtensionPrefs::Get(original_profile)
        ->SetInstallParam(extension_id, install_param);
    // Re-enable the extension if needed.
    if (registry->enabled_extensions().Contains(extension_id)) {
      content::WindowedNotificationObserver load_signal(
          extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
          content::Source<Profile>(original_profile));
      // Reload the extension so that the
      // NOTIFICATION_EXTENSION_LOADED_DEPRECATED
      // observers may access |install_param|.
      service->ReloadExtension(extension_id);
      load_signal.Wait();
      extension = service->GetExtensionById(extension_id, false);
      CHECK(extension) << extension_id << " not found after reloading.";
    }
  }

  // Toggling incognito or file access will reload the extension, so wait for
  // the reload and grab the new extension instance. The default state is
  // incognito disabled and file access enabled, so we don't wait in those
  // cases.
  {
    content::WindowedNotificationObserver load_signal(
        extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
        content::Source<Profile>(original_profile));
    CHECK(!extensions::util::IsIncognitoEnabled(extension_id, original_profile))
        << extension_id << " is enabled in incognito, but shouldn't be";

    if (flags & kFlagEnableIncognito) {
      extensions::util::SetIsIncognitoEnabled(extension_id, original_profile,
                                              true);
      load_signal.Wait();
      extension = service->GetExtensionById(extension_id, false);
      CHECK(extension) << extension_id << " not found after reloading.";
    }
  }

  {
    content::WindowedNotificationObserver load_signal(
        extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
        content::Source<Profile>(original_profile));
    CHECK(extensions::util::AllowFileAccess(extension_id, original_profile));
    if (!(flags & kFlagEnableFileAccess)) {
      extensions::util::SetAllowFileAccess(extension_id, original_profile,
                                           false);
      load_signal.Wait();
      extension = service->GetExtensionById(extension_id, false);
      CHECK(extension) << extension_id << " not found after reloading.";
    }
  }

  if (!observer_->WaitForExtensionViewsToLoad())
    return NULL;

  return extension;
}

const Extension* ExtensionBrowserTest::LoadExtensionAsComponentWithManifest(
    const base::FilePath& path,
    const base::FilePath::CharType* manifest_relative_path) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      profile())->extension_service();
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());

  std::string manifest;
  if (!base::ReadFileToString(path.Append(manifest_relative_path), &manifest)) {
    return NULL;
  }

  service->component_loader()->set_ignore_whitelist_for_testing(true);
  std::string extension_id = service->component_loader()->Add(manifest, path);
  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_id);
  if (!extension)
    return NULL;
  observer_->set_last_loaded_extension_id(extension->id());
  return extension;
}

const Extension* ExtensionBrowserTest::LoadExtensionAsComponent(
    const base::FilePath& path) {
  return LoadExtensionAsComponentWithManifest(path,
                                              extensions::kManifestFilename);
}

const Extension* ExtensionBrowserTest::LoadAndLaunchApp(
    const base::FilePath& path) {
  const Extension* app = LoadExtension(path);
  CHECK(app);
  content::WindowedNotificationObserver app_loaded_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  AppLaunchParams params(profile(), app, extensions::LAUNCH_CONTAINER_NONE,
                         WindowOpenDisposition::NEW_WINDOW,
                         extensions::SOURCE_TEST);
  params.command_line = *base::CommandLine::ForCurrentProcess();
  OpenApplication(params);
  app_loaded_observer.Wait();

  return app;
}

base::FilePath ExtensionBrowserTest::PackExtension(
    const base::FilePath& dir_path) {
  base::FilePath crx_path = temp_dir_.GetPath().AppendASCII("temp.crx");
  if (!base::DeleteFile(crx_path, false)) {
    ADD_FAILURE() << "Failed to delete crx: " << crx_path.value();
    return base::FilePath();
  }

  // Look for PEM files with the same name as the directory.
  base::FilePath pem_path =
      dir_path.ReplaceExtension(FILE_PATH_LITERAL(".pem"));
  base::FilePath pem_path_out;

  if (!base::PathExists(pem_path)) {
    pem_path = base::FilePath();
    pem_path_out = crx_path.DirName().AppendASCII("temp.pem");
    if (!base::DeleteFile(pem_path_out, false)) {
      ADD_FAILURE() << "Failed to delete pem: " << pem_path_out.value();
      return base::FilePath();
    }
  }

  return PackExtensionWithOptions(dir_path, crx_path, pem_path, pem_path_out);
}

base::FilePath ExtensionBrowserTest::PackExtensionWithOptions(
    const base::FilePath& dir_path,
    const base::FilePath& crx_path,
    const base::FilePath& pem_path,
    const base::FilePath& pem_out_path) {
  if (!base::PathExists(dir_path)) {
    ADD_FAILURE() << "Extension dir not found: " << dir_path.value();
    return base::FilePath();
  }

  if (!base::PathExists(pem_path) && pem_out_path.empty()) {
    ADD_FAILURE() << "Must specify a PEM file or PEM output path";
    return base::FilePath();
  }

  std::unique_ptr<ExtensionCreator> creator(new ExtensionCreator());
  if (!creator->Run(dir_path,
                    crx_path,
                    pem_path,
                    pem_out_path,
                    ExtensionCreator::kOverwriteCRX)) {
    ADD_FAILURE() << "ExtensionCreator::Run() failed: "
                  << creator->error_message();
    return base::FilePath();
  }

  if (!base::PathExists(crx_path)) {
    ADD_FAILURE() << crx_path.value() << " was not created.";
    return base::FilePath();
  }
  return crx_path;
}

const Extension* ExtensionBrowserTest::UpdateExtensionWaitForIdle(
    const std::string& id,
    const base::FilePath& path,
    int expected_change) {
  return InstallOrUpdateExtension(id, path, INSTALL_UI_TYPE_NONE,
                                  expected_change, Manifest::INTERNAL,
                                  browser(), Extension::NO_FLAGS, false, false);
}

const Extension* ExtensionBrowserTest::InstallExtensionFromWebstore(
    const base::FilePath& path,
    int expected_change) {
  return InstallOrUpdateExtension(
      std::string(), path, INSTALL_UI_TYPE_AUTO_CONFIRM, expected_change,
      Manifest::INTERNAL, browser(), Extension::FROM_WEBSTORE, true, false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const std::string& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    int expected_change) {
  return InstallOrUpdateExtension(id, path, ui_type, expected_change,
                                  Manifest::INTERNAL, browser(),
                                  Extension::NO_FLAGS, true, false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const std::string& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    int expected_change,
    Browser* browser,
    Extension::InitFromValueFlags creation_flags) {
  return InstallOrUpdateExtension(id, path, ui_type, expected_change,
                                  Manifest::INTERNAL, browser, creation_flags,
                                  true, false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const std::string& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    int expected_change,
    Manifest::Location install_source) {
  return InstallOrUpdateExtension(id, path, ui_type, expected_change,
                                  install_source, browser(),
                                  Extension::NO_FLAGS, true, false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const std::string& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    int expected_change,
    Manifest::Location install_source,
    Browser* browser,
    Extension::InitFromValueFlags creation_flags,
    bool install_immediately,
    bool grant_permissions) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  service->set_show_extensions_prompts(false);
  size_t num_before = registry->enabled_extensions().size();

  {
    std::unique_ptr<ScopedTestDialogAutoConfirm> prompt_auto_confirm;
    if (ui_type == INSTALL_UI_TYPE_CANCEL) {
      prompt_auto_confirm.reset(new ScopedTestDialogAutoConfirm(
          ScopedTestDialogAutoConfirm::CANCEL));
    } else if (ui_type == INSTALL_UI_TYPE_NORMAL) {
      prompt_auto_confirm.reset(new ScopedTestDialogAutoConfirm(
          ScopedTestDialogAutoConfirm::NONE));
    } else if (ui_type == INSTALL_UI_TYPE_AUTO_CONFIRM) {
      prompt_auto_confirm.reset(new ScopedTestDialogAutoConfirm(
          ScopedTestDialogAutoConfirm::ACCEPT));
    }

    // TODO(tessamac): Update callers to always pass an unpacked extension
    //                 and then always pack the extension here.
    base::FilePath crx_path = path;
    if (crx_path.Extension() != FILE_PATH_LITERAL(".crx")) {
      crx_path = PackExtension(path);
    }
    if (crx_path.empty())
      return NULL;

    std::unique_ptr<ExtensionInstallPrompt> install_ui;
    if (prompt_auto_confirm) {
      install_ui.reset(new ExtensionInstallPrompt(
         browser->tab_strip_model()->GetActiveWebContents()));
    }
    scoped_refptr<extensions::CrxInstaller> installer(
        extensions::CrxInstaller::Create(service, std::move(install_ui)));
    installer->set_expected_id(id);
    installer->set_creation_flags(creation_flags);
    installer->set_install_source(install_source);
    installer->set_install_immediately(install_immediately);
    installer->set_allow_silent_install(grant_permissions);
    if (!installer->is_gallery_install()) {
      installer->set_off_store_install_allow_reason(
          extensions::CrxInstaller::OffStoreInstallAllowedInTest);
    }

    observer_->Watch(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::Source<extensions::CrxInstaller>(installer.get()));

    installer->InstallCrx(crx_path);

    observer_->Wait();
  }

  size_t num_after = registry->enabled_extensions().size();
  EXPECT_EQ(num_before + expected_change, num_after);
  if (num_before + expected_change != num_after) {
    VLOG(1) << "Num extensions before: " << base::SizeTToString(num_before)
            << " num after: " << base::SizeTToString(num_after)
            << " Installed extensions follow:";

    for (const scoped_refptr<const Extension>& extension :
         registry->enabled_extensions())
      VLOG(1) << "  " << extension->id();

    VLOG(1) << "Errors follow:";
    const std::vector<base::string16>* errors =
        ExtensionErrorReporter::GetInstance()->GetErrors();
    for (std::vector<base::string16>::const_iterator iter = errors->begin();
         iter != errors->end(); ++iter)
      VLOG(1) << *iter;

    return NULL;
  }

  if (!observer_->WaitForExtensionViewsToLoad())
    return NULL;
  return service->GetExtensionById(last_loaded_extension_id(), false);
}

void ExtensionBrowserTest::ReloadExtension(const std::string& extension_id) {
  observer_->Watch(extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
                   content::NotificationService::AllSources());

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  service->ReloadExtension(extension_id);

  observer_->Wait();
  observer_->WaitForExtensionViewsToLoad();
}

void ExtensionBrowserTest::UnloadExtension(const std::string& extension_id) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      profile())->extension_service();
  service->UnloadExtension(extension_id,
                           extensions::UnloadedExtensionInfo::REASON_DISABLE);
}

void ExtensionBrowserTest::UninstallExtension(const std::string& extension_id) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      profile())->extension_service();
  service->UninstallExtension(extension_id,
                              extensions::UNINSTALL_REASON_FOR_TESTING,
                              base::Bind(&base::DoNothing),
                              NULL);
}

void ExtensionBrowserTest::DisableExtension(const std::string& extension_id) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      profile())->extension_service();
  service->DisableExtension(extension_id, Extension::DISABLE_USER_ACTION);
}

void ExtensionBrowserTest::EnableExtension(const std::string& extension_id) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      profile())->extension_service();
  service->EnableExtension(extension_id);
}

void ExtensionBrowserTest::OpenWindow(content::WebContents* contents,
                                      const GURL& url,
                                      bool newtab_process_should_equal_opener,
                                      content::WebContents** newtab_result) {
  content::WindowedNotificationObserver windowed_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ASSERT_TRUE(content::ExecuteScript(contents,
                                     "window.open('" + url.spec() + "');"));

  // The above window.open call is not user-initiated, so it will create
  // a popup window instead of a new tab in current window.
  // The stop notification will come from the new tab.
  windowed_observer.Wait();
  content::NavigationController* controller =
      content::Source<content::NavigationController>(
          windowed_observer.source()).ptr();
  content::WebContents* newtab = controller->GetWebContents();
  ASSERT_TRUE(newtab);
  EXPECT_EQ(url, controller->GetLastCommittedEntry()->GetURL());
  if (newtab_process_should_equal_opener)
    EXPECT_EQ(contents->GetRenderProcessHost(), newtab->GetRenderProcessHost());
  else
    EXPECT_NE(contents->GetRenderProcessHost(), newtab->GetRenderProcessHost());

  if (newtab_result)
    *newtab_result = newtab;
}

void ExtensionBrowserTest::NavigateInRenderer(content::WebContents* contents,
                                              const GURL& url) {
  // Ensure any existing navigations complete before trying to navigate anew, to
  // avoid triggering of the unload event for the wrong navigation.
  content::WaitForLoadStop(contents);
  bool result = false;
  content::WindowedNotificationObserver windowed_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      contents,
      "window.addEventListener('unload', function() {"
      "    window.domAutomationController.send(true);"
      "}, false);"
      "window.location = '" + url.spec() + "';",
      &result));
  ASSERT_TRUE(result);
  windowed_observer.Wait();
  EXPECT_EQ(url, contents->GetController().GetLastCommittedEntry()->GetURL());
}

extensions::ExtensionHost* ExtensionBrowserTest::FindHostWithPath(
    extensions::ProcessManager* manager,
    const std::string& path,
    int expected_hosts) {
  extensions::ExtensionHost* result_host = nullptr;
  int num_hosts = 0;
  for (extensions::ExtensionHost* host : manager->background_hosts()) {
    if (host->GetURL().path() == path) {
      EXPECT_FALSE(result_host);
      result_host = host;
    }
    num_hosts++;
  }
  EXPECT_EQ(expected_hosts, num_hosts);
  return result_host;
}

std::string ExtensionBrowserTest::ExecuteScriptInBackgroundPage(
    const std::string& extension_id,
    const std::string& script) {
  return extensions::browsertest_util::ExecuteScriptInBackgroundPage(
      profile(), extension_id, script);
}

bool ExtensionBrowserTest::ExecuteScriptInBackgroundPageNoWait(
    const std::string& extension_id,
    const std::string& script) {
  return extensions::browsertest_util::ExecuteScriptInBackgroundPageNoWait(
      profile(), extension_id, script);
}
