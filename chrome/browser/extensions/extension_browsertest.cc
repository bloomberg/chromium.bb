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
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/constants.h"
#include "sync/api/string_ordinal.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

using extensions::Extension;
using extensions::ExtensionCreator;
using extensions::FeatureSwitch;
using extensions::Manifest;

namespace {

bool HasExtensionPageActionCountReachedTarget(LocationBarTesting* location_bar,
                                              int target_page_action_count) {
  VLOG(1) << "Number of page actions: " << location_bar->PageActionCount();
  return location_bar->PageActionCount() == target_page_action_count;
}

bool HasExtensionPageActionVisibilityReachedTarget(
    LocationBarTesting* location_bar,
    int target_visible_page_action_count) {
  VLOG(1) << "Number of visible page actions: "
          << location_bar->PageActionVisibleCount();
  return location_bar->PageActionVisibleCount() ==
         target_visible_page_action_count;
}

}  // namespace

ExtensionBrowserTest::ExtensionBrowserTest()
    : loaded_(false),
      installed_(false),
      extension_installs_observed_(0),
      extension_load_errors_observed_(0),
      current_channel_(chrome::VersionInfo::CHANNEL_DEV),
      override_prompt_for_external_extensions_(
          FeatureSwitch::prompt_for_external_extensions(), false),
      profile_(NULL) {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
}

ExtensionBrowserTest::~ExtensionBrowserTest() {}

Profile* ExtensionBrowserTest::profile() {
  if (!profile_) {
    if (browser())
      profile_ = browser()->profile();
    else
      profile_ = ProfileManager::GetDefaultProfile();
  }
  return profile_;
}

// static
const Extension* ExtensionBrowserTest::GetExtensionByPath(
    const ExtensionSet* extensions, const base::FilePath& path) {
  base::FilePath extension_path = base::MakeAbsoluteFilePath(path);
  EXPECT_TRUE(!extension_path.empty());
  for (ExtensionSet::const_iterator iter = extensions->begin();
       iter != extensions->end(); ++iter) {
    if ((*iter)->path() == extension_path) {
      return iter->get();
    }
  }
  return NULL;
}

void ExtensionBrowserTest::SetUpCommandLine(CommandLine* command_line) {
  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("extensions");

#if defined(OS_CHROMEOS)
  // This makes sure that we create the Default profile first, with no
  // ExtensionService and then the real profile with one, as we do when
  // running on chromeos.
  command_line->AppendSwitchASCII(chromeos::switches::kLoginUser,
                                  "TestUser@gmail.com");
  command_line->AppendSwitchASCII(chromeos::switches::kLoginProfile, "user");
#endif
}

void ExtensionBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
}

const Extension* ExtensionBrowserTest::LoadExtensionWithFlags(
    const base::FilePath& path, int flags) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      profile())->extension_service();
  {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_EXTENSION_LOADED,
        content::NotificationService::AllSources());
    content::NotificationRegistrar registrar;
    registrar.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                  content::NotificationService::AllSources());
    scoped_refptr<extensions::UnpackedInstaller> installer(
        extensions::UnpackedInstaller::Create(service));
    installer->set_prompt_for_plugins(false);
    installer->set_require_modern_manifest_version(
        (flags & kFlagAllowOldManifestVersions) == 0);
    installer->Load(path);
    observer.Wait();
  }

  // Find the loaded extension by its path. See crbug.com/59531 for why
  // we cannot just use last_loaded_extension_id_.
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

      EXPECT_TRUE(extension->install_warnings().empty()) <<
          install_warnings_message;
      return NULL;
    }
  }

  const std::string extension_id = extension->id();

  // The call to OnExtensionInstalled ensures the other extension prefs
  // are set up with the defaults.
  service->extension_prefs()->OnExtensionInstalled(
      extension,
      Extension::ENABLED,
      extensions::Blacklist::NOT_BLACKLISTED,
      syncer::StringOrdinal::CreateInitialOrdinal());

  // Toggling incognito or file access will reload the extension, so wait for
  // the reload and grab the new extension instance. The default state is
  // incognito disabled and file access enabled, so we don't wait in those
  // cases.
  {
    content::WindowedNotificationObserver load_signal(
        chrome::NOTIFICATION_EXTENSION_LOADED,
        content::Source<Profile>(profile()));
    CHECK(!service->IsIncognitoEnabled(extension_id) ||
          extension->force_incognito_enabled());

    if (flags & kFlagEnableIncognito) {
      service->SetIsIncognitoEnabled(extension_id, true);
      load_signal.Wait();
      extension = service->GetExtensionById(extension_id, false);
      CHECK(extension) << extension_id << " not found after reloading.";
    }
  }

  {
    content::WindowedNotificationObserver load_signal(
        chrome::NOTIFICATION_EXTENSION_LOADED,
        content::Source<Profile>(profile()));
    CHECK(service->AllowFileAccess(extension));
    if (!(flags & kFlagEnableFileAccess)) {
      service->SetAllowFileAccess(extension, false);
      load_signal.Wait();
      extension = service->GetExtensionById(extension_id, false);
      CHECK(extension) << extension_id << " not found after reloading.";
    }
  }

  if (!WaitForExtensionViewsToLoad())
    return NULL;

  return extension;
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
  last_loaded_extension_id_ = extension->id();
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
                                  false,
                                  true);
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
                                  true,
                                  false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const std::string& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    int expected_change) {
  return InstallOrUpdateExtension(id, path, ui_type, expected_change,
                                  Manifest::INTERNAL, browser(), false, false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const std::string& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    int expected_change,
    Browser* browser,
    bool from_webstore) {
  return InstallOrUpdateExtension(id, path, ui_type, expected_change,
                                  Manifest::INTERNAL, browser, from_webstore,
                                  false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const std::string& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    int expected_change,
    Manifest::Location install_source) {
  return InstallOrUpdateExtension(id, path, ui_type, expected_change,
                                  install_source, browser(), false, false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const std::string& id,
    const base::FilePath& path,
    InstallUIType ui_type,
    int expected_change,
    Manifest::Location install_source,
    Browser* browser,
    bool from_webstore,
    bool wait_for_idle) {
  ExtensionService* service = profile()->GetExtensionService();
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
    installer->set_is_gallery_install(from_webstore);
    installer->set_install_source(install_source);
    installer->set_install_wait_for_idle(wait_for_idle);
    if (!from_webstore) {
      installer->set_off_store_install_allow_reason(
          extensions::CrxInstaller::OffStoreInstallAllowedInTest);
    }

    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_CRX_INSTALLER_DONE,
        content::Source<extensions::CrxInstaller>(installer.get()));
    content::NotificationRegistrar registrar;
    registrar.Add(this, chrome::NOTIFICATION_CRX_INSTALLER_DONE,
                  content::Source<extensions::CrxInstaller>(installer.get()));

    installer->InstallCrx(crx_path);

    observer.Wait();
  }

  size_t num_after = service->extensions()->size();
  EXPECT_EQ(num_before + expected_change, num_after);
  if (num_before + expected_change != num_after) {
    VLOG(1) << "Num extensions before: " << base::IntToString(num_before)
            << " num after: " << base::IntToString(num_after)
            << " Installed extensions follow:";

    for (ExtensionSet::const_iterator it = service->extensions()->begin();
         it != service->extensions()->end(); ++it)
      VLOG(1) << "  " << (*it)->id();

    VLOG(1) << "Errors follow:";
    const std::vector<string16>* errors =
        ExtensionErrorReporter::GetInstance()->GetErrors();
    for (std::vector<string16>::const_iterator iter = errors->begin();
         iter != errors->end(); ++iter)
      VLOG(1) << *iter;

    return NULL;
  }

  if (!WaitForExtensionViewsToLoad())
    return NULL;
  return service->GetExtensionById(last_loaded_extension_id_, false);
}

void ExtensionBrowserTest::ReloadExtension(const std::string extension_id) {
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_EXTENSION_LOADED,
      content::NotificationService::AllSources());
  content::NotificationRegistrar registrar;
  registrar.Add(this,
                chrome::NOTIFICATION_EXTENSION_LOADED,
                content::NotificationService::AllSources());
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile())->extension_service();
  service->ReloadExtension(extension_id);
  observer.Wait();
}

void ExtensionBrowserTest::UnloadExtension(const std::string& extension_id) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      profile())->extension_service();
  service->UnloadExtension(extension_id, extension_misc::UNLOAD_REASON_DISABLE);
}

void ExtensionBrowserTest::UninstallExtension(const std::string& extension_id) {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      profile())->extension_service();
  service->UninstallExtension(extension_id, false, NULL);
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

bool ExtensionBrowserTest::WaitForPageActionCountChangeTo(int count) {
  LocationBarTesting* location_bar =
      browser()->window()->GetLocationBar()->GetLocationBarForTesting();
  if (!HasExtensionPageActionCountReachedTarget(location_bar, count)) {
    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        base::Bind(
            &HasExtensionPageActionCountReachedTarget, location_bar, count))
        .Wait();
  }
  return HasExtensionPageActionCountReachedTarget(location_bar, count);
}

bool ExtensionBrowserTest::WaitForPageActionVisibilityChangeTo(int count) {
  LocationBarTesting* location_bar =
      browser()->window()->GetLocationBar()->GetLocationBarForTesting();
  if (!HasExtensionPageActionVisibilityReachedTarget(location_bar, count)) {
    content::WindowedNotificationObserver(
        chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
        base::Bind(&HasExtensionPageActionVisibilityReachedTarget,
                   location_bar,
                   count))
        .Wait();
  }
  return HasExtensionPageActionVisibilityReachedTarget(location_bar, count);
}

void ExtensionBrowserTest::WaitForNotification(int notification_type) {
  // TODO(bauerb): Using a WindowedNotificationObserver like this can break
  // easily, if the notification we're waiting for is sent before this method.
  // Change it so that the WindowedNotificationObserver is constructed earlier.
  content::NotificationRegistrar registrar;
  registrar.Add(this, notification_type,
                content::NotificationService::AllSources());
  content::WindowedNotificationObserver(
      notification_type, content::NotificationService::AllSources()).Wait();
}

bool ExtensionBrowserTest::WaitForExtensionViewsToLoad() {

  ExtensionProcessManager* manager =
      extensions::ExtensionSystem::Get(profile())->process_manager();
  ExtensionProcessManager::ViewSet all_views = manager->GetAllViews();
  for (ExtensionProcessManager::ViewSet::const_iterator iter =
           all_views.begin();
       iter != all_views.end();) {
    if (!(*iter)->IsLoading()) {
      ++iter;
    } else {
      // Wait for all the extension render view hosts that exist to finish
      // loading.
      content::WindowedNotificationObserver observer(
          content::NOTIFICATION_LOAD_STOP,
          content::NotificationService::AllSources());
      observer.AddNotificationType(
          content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
          content::NotificationService::AllSources());
      observer.Wait();

      // Test activity may have modified the set of extension processes during
      // message processing, so re-start the iteration to catch added/removed
      // processes.
      all_views = manager->GetAllViews();
      iter = all_views.begin();
    }
  }
  return true;
}

bool ExtensionBrowserTest::WaitForExtensionInstall() {
  int before = extension_installs_observed_;
  WaitForNotification(chrome::NOTIFICATION_EXTENSION_INSTALLED);
  return extension_installs_observed_ == (before + 1);
}

bool ExtensionBrowserTest::WaitForExtensionInstallError() {
  int before = extension_installs_observed_;
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR,
      content::NotificationService::AllSources()).Wait();
  return extension_installs_observed_ == before;
}

void ExtensionBrowserTest::WaitForExtensionLoad() {
  WaitForNotification(chrome::NOTIFICATION_EXTENSION_LOADED);
  WaitForExtensionViewsToLoad();
}

bool ExtensionBrowserTest::WaitForExtensionLoadError() {
  int before = extension_load_errors_observed_;
  WaitForNotification(chrome::NOTIFICATION_EXTENSION_LOAD_ERROR);
  return extension_load_errors_observed_ != before;
}

bool ExtensionBrowserTest::WaitForExtensionCrash(
    const std::string& extension_id) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile())->extension_service();

  if (!service->GetExtensionById(extension_id, true)) {
    // The extension is already unloaded, presumably due to a crash.
    return true;
  }
  content::WindowedNotificationObserver(
      chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
      content::NotificationService::AllSources()).Wait();
  return (service->GetExtensionById(extension_id, true) == NULL);
}

bool ExtensionBrowserTest::WaitForCrxInstallerDone() {
  int before = crx_installers_done_observed_;
  WaitForNotification(chrome::NOTIFICATION_CRX_INSTALLER_DONE);
  return crx_installers_done_observed_ == (before + 1);
}

void ExtensionBrowserTest::OpenWindow(content::WebContents* contents,
                                      const GURL& url,
                                      bool newtab_process_should_equal_opener,
                                      content::WebContents** newtab_result) {
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  ASSERT_TRUE(content::ExecuteScript(contents,
                                     "window.open('" + url.spec() + "');"));

  // The above window.open call is not user-initiated, so it will create
  // a popup window instead of a new tab in current window.
  // The stop notification will come from the new tab.
  observer.Wait();
  content::NavigationController* controller =
      content::Source<content::NavigationController>(observer.source()).ptr();
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
  content::WindowedNotificationObserver observer(
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
  observer.Wait();
  EXPECT_EQ(url, contents->GetController().GetLastCommittedEntry()->GetURL());
}

extensions::ExtensionHost* ExtensionBrowserTest::FindHostWithPath(
    ExtensionProcessManager* manager,
    const std::string& path,
    int expected_hosts) {
  extensions::ExtensionHost* host = NULL;
  int num_hosts = 0;
  ExtensionProcessManager::ExtensionHostSet background_hosts =
      manager->background_hosts();
  for (ExtensionProcessManager::const_iterator iter = background_hosts.begin();
       iter != background_hosts.end(); ++iter) {
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

void ExtensionBrowserTest::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED:
      last_loaded_extension_id_ =
          content::Details<const Extension>(details).ptr()->id();
      VLOG(1) << "Got EXTENSION_LOADED notification.";
      break;

    case chrome::NOTIFICATION_CRX_INSTALLER_DONE:
      VLOG(1) << "Got CRX_INSTALLER_DONE notification.";
      {
        const Extension* extension =
            content::Details<const Extension>(details).ptr();
        if (extension)
          last_loaded_extension_id_ = extension->id();
        else
          last_loaded_extension_id_.clear();
      }
      ++crx_installers_done_observed_;
      break;

    case chrome::NOTIFICATION_EXTENSION_INSTALLED:
      VLOG(1) << "Got EXTENSION_INSTALLED notification.";
      ++extension_installs_observed_;
      break;

    case chrome::NOTIFICATION_EXTENSION_LOAD_ERROR:
      VLOG(1) << "Got EXTENSION_LOAD_ERROR notification.";
      ++extension_load_errors_observed_;
      break;

    default:
      NOTREACHED();
      break;
  }
}
