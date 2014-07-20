// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"

#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_creator.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/extensions/updater/extension_cache_fake.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_set.h"
#include "sync/api/string_ordinal.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

using extensions::Extension;
using extensions::ExtensionCreator;
using extensions::FeatureSwitch;
using extensions::Manifest;

ExtensionBrowserTest::ExtensionBrowserTest()
    : loaded_(false),
      installed_(false),
#if defined(OS_CHROMEOS)
      set_chromeos_user_(true),
#endif
      // Default channel is STABLE but override with UNKNOWN so that unlaunched
      // or incomplete APIs can write tests.
      current_channel_(chrome::VersionInfo::CHANNEL_UNKNOWN),
      override_prompt_for_external_extensions_(
          FeatureSwitch::prompt_for_external_extensions(),
          false),
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
    const extensions::ExtensionSet* extensions, const base::FilePath& path) {
  base::FilePath extension_path = base::MakeAbsoluteFilePath(path);
  EXPECT_TRUE(!extension_path.empty());
  for (extensions::ExtensionSet::const_iterator iter = extensions->begin();
       iter != extensions->end(); ++iter) {
    if ((*iter)->path() == extension_path) {
      return iter->get();
    }
  }
  return NULL;
}

void ExtensionBrowserTest::SetUp() {
  test_extension_cache_.reset(new extensions::ExtensionCacheFake());
  InProcessBrowserTest::SetUp();
}

void ExtensionBrowserTest::SetUpCommandLine(CommandLine* command_line) {
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("extensions");
  observer_.reset(new ExtensionTestNotificationObserver(browser()));

#if defined(OS_CHROMEOS)
  if (set_chromeos_user_) {
    // This makes sure that we create the Default profile first, with no
    // ExtensionService and then the real profile with one, as we do when
    // running on chromeos.
    command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                    "TestUser@gmail.com");
    command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
  }
#endif
}

void ExtensionBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  observer_.reset(new ExtensionTestNotificationObserver(browser()));
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
  {
    observer_->Watch(chrome::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
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
  const Extension* extension = GetExtensionByPath(service->extensions(), path);
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

  if (!install_param.empty()) {
    extensions::ExtensionPrefs::Get(profile())
        ->SetInstallParam(extension_id, install_param);
    // Re-enable the extension if needed.
    if (service->extensions()->Contains(extension_id)) {
      content::WindowedNotificationObserver load_signal(
          chrome::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
          content::Source<Profile>(profile()));
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
        chrome::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
        content::Source<Profile>(profile()));
    CHECK(!extensions::util::IsIncognitoEnabled(extension_id, profile()));

    if (flags & kFlagEnableIncognito) {
      extensions::util::SetIsIncognitoEnabled(extension_id, profile(), true);
      load_signal.Wait();
      extension = service->GetExtensionById(extension_id, false);
      CHECK(extension) << extension_id << " not found after reloading.";
    }
  }

  {
    content::WindowedNotificationObserver load_signal(
        chrome::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
        content::Source<Profile>(profile()));
    CHECK(extensions::util::AllowFileAccess(extension_id, profile()));
    if (!(flags & kFlagEnableFileAccess)) {
      extensions::util::SetAllowFileAccess(extension_id, profile(), false);
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

  std::string manifest;
  if (!base::ReadFileToString(path.Append(manifest_relative_path), &manifest)) {
    return NULL;
  }

  std::string extension_id = service->component_loader()->Add(manifest, path);
  const Extension* extension = service->extensions()->GetByID(extension_id);
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

base::FilePath ExtensionBrowserTest::PackExtension(
    const base::FilePath& dir_path) {
  base::FilePath crx_path = temp_dir_.path().AppendASCII("temp.crx");
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

  scoped_ptr<ExtensionCreator> creator(new ExtensionCreator());
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

// This class is used to simulate an installation abort by the user.
class MockAbortExtensionInstallPrompt : public ExtensionInstallPrompt {
 public:
  MockAbortExtensionInstallPrompt() : ExtensionInstallPrompt(NULL) {
  }

  // Simulate a user abort on an extension installation.
  virtual void ConfirmInstall(
      Delegate* delegate,
      const Extension* extension,
      const ShowDialogCallback& show_dialog_callback) OVERRIDE {
    delegate->InstallUIAbort(true);
    base::MessageLoopForUI::current()->Quit();
  }

  virtual void OnInstallSuccess(const Extension* extension,
                                SkBitmap* icon) OVERRIDE {}

  virtual void OnInstallFailure(
      const extensions::CrxInstallerError& error) OVERRIDE {}
};

class MockAutoConfirmExtensionInstallPrompt : public ExtensionInstallPrompt {
 public:
  explicit MockAutoConfirmExtensionInstallPrompt(
      content::WebContents* web_contents)
    : ExtensionInstallPrompt(web_contents) {}

  // Proceed without confirmation prompt.
  virtual void ConfirmInstall(
      Delegate* delegate,
      const Extension* extension,
      const ShowDialogCallback& show_dialog_callback) OVERRIDE {
    delegate->InstallUIProceed();
  }
};

const Extension* ExtensionBrowserTest::UpdateExtensionWaitForIdle(
    const std::string& id,
    const base::FilePath& path,
    int expected_change) {
  return InstallOrUpdateExtension(id,
                                  path,
                                  INSTALL_UI_TYPE_NONE,
                                  expected_change,
                                  Manifest::INTERNAL,
                                  browser(),
                                  Extension::NO_FLAGS,
                                  false,
                                  false);
}

const Extension* ExtensionBrowserTest::InstallExtensionFromWebstore(
    const base::FilePath& path,
    int expected_change) {
  return InstallOrUpdateExtension(std::string(),
                                  path,
                                  INSTALL_UI_TYPE_NONE,
                                  expected_change,
                                  Manifest::INTERNAL,
                                  browser(),
                                  Extension::FROM_WEBSTORE,
                                  true,
                                  false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const std::string& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    int expected_change) {
  return InstallOrUpdateExtension(id,
                                  path,
                                  ui_type,
                                  expected_change,
                                  Manifest::INTERNAL,
                                  browser(),
                                  Extension::NO_FLAGS,
                                  true,
                                  false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const std::string& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    int expected_change,
    Browser* browser,
    Extension::InitFromValueFlags creation_flags) {
  return InstallOrUpdateExtension(id,
                                  path,
                                  ui_type,
                                  expected_change,
                                  Manifest::INTERNAL,
                                  browser,
                                  creation_flags,
                                  true,
                                  false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const std::string& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    int expected_change,
    Manifest::Location install_source) {
  return InstallOrUpdateExtension(id,
                                  path,
                                  ui_type,
                                  expected_change,
                                  install_source,
                                  browser(),
                                  Extension::NO_FLAGS,
                                  true,
                                  false);
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
    bool is_ephemeral) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  service->set_show_extensions_prompts(false);
  size_t num_before = service->extensions()->size();

  {
    scoped_ptr<ExtensionInstallPrompt> install_ui;
    if (ui_type == INSTALL_UI_TYPE_CANCEL) {
      install_ui.reset(new MockAbortExtensionInstallPrompt());
    } else if (ui_type == INSTALL_UI_TYPE_NORMAL) {
      install_ui.reset(new ExtensionInstallPrompt(
          browser->tab_strip_model()->GetActiveWebContents()));
    } else if (ui_type == INSTALL_UI_TYPE_AUTO_CONFIRM) {
      install_ui.reset(new MockAutoConfirmExtensionInstallPrompt(
          browser->tab_strip_model()->GetActiveWebContents()));
    }

    // TODO(tessamac): Update callers to always pass an unpacked extension
    //                 and then always pack the extension here.
    base::FilePath crx_path = path;
    if (crx_path.Extension() != FILE_PATH_LITERAL(".crx")) {
      crx_path = PackExtension(path);
    }
    if (crx_path.empty())
      return NULL;

    scoped_refptr<extensions::CrxInstaller> installer(
        extensions::CrxInstaller::Create(service, install_ui.Pass()));
    installer->set_expected_id(id);
    installer->set_creation_flags(creation_flags);
    installer->set_install_source(install_source);
    installer->set_install_immediately(install_immediately);
    installer->set_is_ephemeral(is_ephemeral);
    if (!installer->is_gallery_install()) {
      installer->set_off_store_install_allow_reason(
          extensions::CrxInstaller::OffStoreInstallAllowedInTest);
    }

    observer_->Watch(
        chrome::NOTIFICATION_CRX_INSTALLER_DONE,
        content::Source<extensions::CrxInstaller>(installer.get()));

    installer->InstallCrx(crx_path);

    observer_->Wait();
  }

  size_t num_after = service->extensions()->size();
  EXPECT_EQ(num_before + expected_change, num_after);
  if (num_before + expected_change != num_after) {
    VLOG(1) << "Num extensions before: " << base::IntToString(num_before)
            << " num after: " << base::IntToString(num_after)
            << " Installed extensions follow:";

    for (extensions::ExtensionSet::const_iterator it =
             service->extensions()->begin();
         it != service->extensions()->end(); ++it)
      VLOG(1) << "  " << (*it)->id();

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

void ExtensionBrowserTest::ReloadExtension(const std::string extension_id) {
  observer_->Watch(chrome::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
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
  service->UninstallExtension(
      extension_id, extensions::UNINSTALL_REASON_FOR_TESTING, NULL);
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
  extensions::ExtensionHost* host = NULL;
  int num_hosts = 0;
  extensions::ProcessManager::ExtensionHostSet background_hosts =
      manager->background_hosts();
  for (extensions::ProcessManager::const_iterator iter =
           background_hosts.begin();
       iter != background_hosts.end();
       ++iter) {
    if ((*iter)->GetURL().path() == path) {
      EXPECT_FALSE(host);
      host = *iter;
    }
    num_hosts++;
  }
  EXPECT_EQ(expected_hosts, num_hosts);
  return host;
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
