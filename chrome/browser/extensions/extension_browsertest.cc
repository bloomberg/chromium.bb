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
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
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
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_dialog_auto_confirm.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "net/url_request/url_request_file_job.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

using extensions::Extension;
using extensions::ExtensionCreator;
using extensions::ExtensionRegistry;
using extensions::FeatureSwitch;
using extensions::Manifest;
using extensions::ScopedTestDialogAutoConfirm;

namespace {

// Maps all chrome-extension://<id>/_test_resources/foo requests to
// chrome/test/data/extensions/foo. This is what allows us to share code between
// tests without needing to duplicate files in each extension.
net::URLRequestJob* ExtensionProtocolTestHandler(
    const base::FilePath& test_dir_root,
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const base::FilePath& relative_path) {
  // Only map paths that begin with _test_resources.
  if (!base::FilePath(FILE_PATH_LITERAL("_test_resources"))
           .IsParent(relative_path)) {
    return nullptr;
  }

  // Replace _test_resources/foo with chrome/test/data/foo.
  std::vector<base::FilePath::StringType> components;
  relative_path.GetComponents(&components);
  DCHECK_GT(components.size(), 1u);
  base::FilePath resource_path = test_dir_root;
  for (size_t i = 1u; i < components.size(); ++i)
    resource_path = resource_path.Append(components[i]);

  return new net::URLRequestFileJob(
      request, network_delegate, resource_path,
      base::CreateTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}));
}

}  // namespace

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
  base::ThreadRestrictions::ScopedAllowIO allow_io;
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
  observer_.reset(
      new extensions::ChromeExtensionTestNotificationObserver(browser()));
  if (extension_service()->updater()) {
    extension_service()->updater()->SetExtensionCacheForTesting(
        test_extension_cache_.get());
  }

  // We don't use test_data_dir_ here because we want this to point to
  // chrome/test/data/extensions, and subclasses have a nasty habit of altering
  // the data dir in SetUpCommandLine().
  base::FilePath test_root_path;
  PathService::Get(chrome::DIR_TEST_DATA, &test_root_path);
  test_root_path = test_root_path.AppendASCII("extensions");
  test_protocol_handler_ =
      base::Bind(&ExtensionProtocolTestHandler, test_root_path);
  extensions::SetExtensionProtocolTestHandler(&test_protocol_handler_);
}

void ExtensionBrowserTest::TearDownOnMainThread() {
  ExtensionMessageBubbleFactory::set_override_for_tests(
      ExtensionMessageBubbleFactory::NO_OVERRIDE);
  extensions::SetExtensionProtocolTestHandler(nullptr);
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

const Extension* ExtensionBrowserTest::LoadExtensionWithInstallParam(
    const base::FilePath& path,
    int flags,
    const std::string& install_param) {
  extensions::ChromeTestExtensionLoader loader(profile());
  loader.set_require_modern_manifest_version(
      (flags & kFlagAllowOldManifestVersions) == 0);
  loader.set_ignore_manifest_warnings(
      (flags & kFlagIgnoreManifestWarnings) != 0);
  loader.set_allow_incognito_access((flags & kFlagEnableIncognito) != 0);
  loader.set_allow_file_access((flags & kFlagEnableFileAccess) != 0);
  loader.set_install_param(install_param);
  scoped_refptr<const Extension> extension = loader.LoadExtension(path);
  if (extension)
    observer_->set_last_loaded_extension_id(extension->id());
  return extension.get();
}

const Extension* ExtensionBrowserTest::LoadExtensionAsComponentWithManifest(
    const base::FilePath& path,
    const base::FilePath::CharType* manifest_relative_path) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      profile())->extension_service();
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());

  base::ThreadRestrictions::ScopedAllowIO allow_io;
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
  base::ThreadRestrictions::ScopedAllowIO allow_io;
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
  base::ThreadRestrictions::ScopedAllowIO allow_io;
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
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  const Extension* extension = registry->GetInstalledExtension(extension_id);
  ASSERT_TRUE(extension);
  extensions::TestExtensionRegistryObserver observer(registry, extension_id);
  extensions::ExtensionSystem::Get(profile())
      ->extension_service()
      ->ReloadExtension(extension_id);
  observer.WaitForExtensionLoaded();

  // We need to let other ExtensionRegistryObservers handle the extension load
  // in order to finish initialization. This has to be done before waiting for
  // extension views to load, since we only register views after observing
  // extension load.
  base::RunLoop().RunUntilIdle();
  observer_->WaitForExtensionViewsToLoad();
}

void ExtensionBrowserTest::UnloadExtension(const std::string& extension_id) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      profile())->extension_service();
  service->UnloadExtension(extension_id,
                           extensions::UnloadedExtensionReason::DISABLE);
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
  content::WebContentsAddedObserver tab_added_observer;
  ASSERT_TRUE(content::ExecuteScript(contents,
                                     "window.open('" + url.spec() + "');"));
  content::WebContents* newtab = tab_added_observer.GetWebContents();
  ASSERT_TRUE(newtab);
  WaitForLoadStop(newtab);
  EXPECT_EQ(url, newtab->GetLastCommittedURL());
  if (newtab_process_should_equal_opener) {
    EXPECT_EQ(contents->GetMainFrame()->GetSiteInstance(),
              newtab->GetMainFrame()->GetSiteInstance());
  } else {
    EXPECT_NE(contents->GetMainFrame()->GetSiteInstance(),
              newtab->GetMainFrame()->GetSiteInstance());
  }

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
