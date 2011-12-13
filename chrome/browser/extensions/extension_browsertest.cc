// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"

#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_creator.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

ExtensionBrowserTest::ExtensionBrowserTest()
    : loaded_(false),
      installed_(false),
      extension_installs_observed_(0),
      target_page_action_count_(-1),
      target_visible_page_action_count_(-1) {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
}

void ExtensionBrowserTest::SetUpCommandLine(CommandLine* command_line) {
  // This enables DOM automation for tab contentses.
  EnableDOMAutomation();

  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("extensions");

#if defined(OS_CHROMEOS)
  // This makes sure that we create the Default profile first, with no
  // ExtensionService and then the real profile with one, as we do when
  // running on chromeos.
  command_line->AppendSwitchASCII(switches::kLoginUser,
                                  "TestUser@gmail.com");
  command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
  command_line->AppendSwitch(switches::kNoFirstRun);
#endif
}

const Extension* ExtensionBrowserTest::LoadExtensionWithOptions(
    const FilePath& path, bool incognito_enabled, bool fileaccess_enabled) {
  ExtensionService* service = browser()->profile()->GetExtensionService();
  {
    content::NotificationRegistrar registrar;
    registrar.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                  content::NotificationService::AllSources());
    scoped_refptr<extensions::UnpackedInstaller> installer(
        extensions::UnpackedInstaller::Create(service));
    installer->set_prompt_for_plugins(false);
    installer->Load(path);
    ui_test_utils::RunMessageLoop();
  }

  // Find the loaded extension by its path. See crbug.com/59531 for why
  // we cannot just use last_loaded_extension_id_.
  FilePath extension_path = path;
  file_util::AbsolutePath(&extension_path);
  const Extension* extension = NULL;
  for (ExtensionSet::const_iterator iter = service->extensions()->begin();
       iter != service->extensions()->end(); ++iter) {
    if ((*iter)->path() == extension_path) {
      extension = *iter;
      break;
    }
  }
  if (!extension)
    return NULL;

  const std::string extension_id = extension->id();

  // The call to OnExtensionInstalled ensures the other extension prefs
  // are set up with the defaults.
  service->extension_prefs()->OnExtensionInstalled(
      extension, Extension::ENABLED, false, 0);

  // Toggling incognito or file access will reload the extension, so wait for
  // the reload and grab the new extension instance. The default state is
  // incognito disabled and file access enabled, so we don't wait in those
  // cases.
  {
    ui_test_utils::WindowedNotificationObserver load_signal(
        chrome::NOTIFICATION_EXTENSION_LOADED,
        content::Source<Profile>(browser()->profile()));
    CHECK(!service->IsIncognitoEnabled(extension_id));

    if (incognito_enabled) {
      service->SetIsIncognitoEnabled(extension_id, incognito_enabled);
      load_signal.Wait();
      extension = service->GetExtensionById(extension_id, false);
      CHECK(extension) << extension_id << " not found after reloading.";
    }
  }

  {
    ui_test_utils::WindowedNotificationObserver load_signal(
        chrome::NOTIFICATION_EXTENSION_LOADED,
        content::Source<Profile>(browser()->profile()));
    CHECK(service->AllowFileAccess(extension));

    if (!fileaccess_enabled) {
      service->SetAllowFileAccess(extension, fileaccess_enabled);
      load_signal.Wait();
      extension = service->GetExtensionById(extension_id, false);
      CHECK(extension) << extension_id << " not found after reloading.";
    }
  }

  if (!WaitForExtensionHostsToLoad())
    return NULL;

  return extension;
}

const Extension* ExtensionBrowserTest::LoadExtension(const FilePath& path) {
  return LoadExtensionWithOptions(path, false, true);
}

const Extension* ExtensionBrowserTest::LoadExtensionIncognito(
    const FilePath& path) {
  return LoadExtensionWithOptions(path, true, true);
}

bool ExtensionBrowserTest::LoadExtensionAsComponent(const FilePath& path) {
  ExtensionService* service = browser()->profile()->GetExtensionService();

  std::string manifest;
  if (!file_util::ReadFileToString(path.Append(Extension::kManifestFilename),
                                   &manifest))
    return false;

  service->component_loader()->Add(manifest, path);

  return true;
}

FilePath ExtensionBrowserTest::PackExtension(const FilePath& dir_path) {
  FilePath crx_path = temp_dir_.path().AppendASCII("temp.crx");
  if (!file_util::Delete(crx_path, false)) {
    ADD_FAILURE() << "Failed to delete crx: " << crx_path.value();
    return FilePath();
  }

  FilePath pem_path = crx_path.DirName().AppendASCII("temp.pem");
  if (!file_util::Delete(pem_path, false)) {
    ADD_FAILURE() << "Failed to delete pem: " << pem_path.value();
    return FilePath();
  }

  return PackExtensionWithOptions(dir_path, crx_path, FilePath(), pem_path);
}

FilePath ExtensionBrowserTest::PackExtensionWithOptions(
    const FilePath& dir_path,
    const FilePath& crx_path,
    const FilePath& pem_path,
    const FilePath& pem_out_path) {
  if (!file_util::PathExists(dir_path)) {
    ADD_FAILURE() << "Extension dir not found: " << dir_path.value();
    return FilePath();
  }

  if (!file_util::PathExists(pem_path) && pem_out_path.empty()) {
    ADD_FAILURE() << "Must specify a PEM file or PEM output path";
    return FilePath();
  }

  scoped_ptr<ExtensionCreator> creator(new ExtensionCreator());
  if (!creator->Run(dir_path,
                    crx_path,
                    pem_path,
                    pem_out_path)) {
    ADD_FAILURE() << "ExtensionCreator::Run() failed: "
                  << creator->error_message();
    return FilePath();
  }

  if (!file_util::PathExists(crx_path)) {
    ADD_FAILURE() << crx_path.value() << " was not created.";
    return FilePath();
  }
  return crx_path;
}

// This class is used to simulate an installation abort by the user.
class MockAbortExtensionInstallUI : public ExtensionInstallUI {
 public:
  MockAbortExtensionInstallUI() : ExtensionInstallUI(NULL) {}

  // Simulate a user abort on an extension installation.
  virtual void ConfirmInstall(Delegate* delegate, const Extension* extension) {
    delegate->InstallUIAbort(true);
    MessageLoopForUI::current()->Quit();
  }

  virtual void OnInstallSuccess(const Extension* extension, SkBitmap* icon) {}

  virtual void OnInstallFailure(const std::string& error) {}
};

class MockAutoConfirmExtensionInstallUI : public ExtensionInstallUI {
 public:
  explicit MockAutoConfirmExtensionInstallUI(Profile* profile) :
      ExtensionInstallUI(profile) {}

  // Proceed without confirmation prompt.
  virtual void ConfirmInstall(Delegate* delegate, const Extension* extension) {
    delegate->InstallUIProceed();
  }
};

const Extension* ExtensionBrowserTest::InstallExtensionFromWebstore(
    const FilePath& path,
    int expected_change) {
  return InstallOrUpdateExtension("", path, INSTALL_UI_TYPE_NONE,
                                  expected_change, browser()->profile(),
                                  true);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const std::string& id,
    const FilePath& path,
    InstallUIType ui_type,
    int expected_change) {
  return InstallOrUpdateExtension(id, path, ui_type, expected_change,
                                  browser()->profile(), false);
}

const Extension* ExtensionBrowserTest::InstallOrUpdateExtension(
    const std::string& id,
    const FilePath& path,
    InstallUIType ui_type,
    int expected_change,
    Profile* profile,
    bool from_webstore) {
  ExtensionService* service = profile->GetExtensionService();
  service->set_show_extensions_prompts(false);
  size_t num_before = service->extensions()->size();

  {
    ExtensionInstallUI* install_ui = NULL;
    if (ui_type == INSTALL_UI_TYPE_CANCEL)
      install_ui = new MockAbortExtensionInstallUI();
    else if (ui_type == INSTALL_UI_TYPE_NORMAL)
      install_ui = new ExtensionInstallUI(profile);
    else if (ui_type == INSTALL_UI_TYPE_AUTO_CONFIRM)
      install_ui = new MockAutoConfirmExtensionInstallUI(profile);

    // TODO(tessamac): Update callers to always pass an unpacked extension
    //                 and then always pack the extension here.
    FilePath crx_path = path;
    if (crx_path.Extension() != FILE_PATH_LITERAL(".crx")) {
      crx_path = PackExtension(path);
    }
    if (crx_path.empty())
      return NULL;

    scoped_refptr<CrxInstaller> installer(
        CrxInstaller::Create(service, install_ui));
    installer->set_expected_id(id);
    installer->set_is_gallery_install(from_webstore);

    content::NotificationRegistrar registrar;
    registrar.Add(this, chrome::NOTIFICATION_CRX_INSTALLER_DONE,
                  content::Source<CrxInstaller>(installer.get()));

    installer->InstallCrx(crx_path);

    ui_test_utils::RunMessageLoop();
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
    const std::vector<std::string>* errors =
        ExtensionErrorReporter::GetInstance()->GetErrors();
    for (std::vector<std::string>::const_iterator iter = errors->begin();
         iter != errors->end(); ++iter)
      VLOG(1) << *iter;

    return NULL;
  }

  if (!WaitForExtensionHostsToLoad())
    return NULL;
  return service->GetExtensionById(last_loaded_extension_id_, false);
}

void ExtensionBrowserTest::ReloadExtension(const std::string& extension_id) {
  ExtensionService* service = browser()->profile()->GetExtensionService();
  service->ReloadExtension(extension_id);
  ui_test_utils::RegisterAndWait(this,
                                 chrome::NOTIFICATION_EXTENSION_LOADED,
                                 content::NotificationService::AllSources());
}

void ExtensionBrowserTest::UnloadExtension(const std::string& extension_id) {
  ExtensionService* service = browser()->profile()->GetExtensionService();
  service->UnloadExtension(extension_id, extension_misc::UNLOAD_REASON_DISABLE);
}

void ExtensionBrowserTest::UninstallExtension(const std::string& extension_id) {
  ExtensionService* service = browser()->profile()->GetExtensionService();
  service->UninstallExtension(extension_id, false, NULL);
}

void ExtensionBrowserTest::DisableExtension(const std::string& extension_id) {
  ExtensionService* service = browser()->profile()->GetExtensionService();
  service->DisableExtension(extension_id);
}

void ExtensionBrowserTest::EnableExtension(const std::string& extension_id) {
  ExtensionService* service = browser()->profile()->GetExtensionService();
  service->EnableExtension(extension_id);
}

bool ExtensionBrowserTest::WaitForPageActionCountChangeTo(int count) {
  LocationBarTesting* location_bar =
      browser()->window()->GetLocationBar()->GetLocationBarForTesting();
  if (location_bar->PageActionCount() != count) {
    target_page_action_count_ = count;
    ui_test_utils::RegisterAndWait(this,
        chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        content::NotificationService::AllSources());
  }
  return location_bar->PageActionCount() == count;
}

bool ExtensionBrowserTest::WaitForPageActionVisibilityChangeTo(int count) {
  LocationBarTesting* location_bar =
      browser()->window()->GetLocationBar()->GetLocationBarForTesting();
  if (location_bar->PageActionVisibleCount() != count) {
    target_visible_page_action_count_ = count;
    ui_test_utils::RegisterAndWait(this,
        chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
        content::NotificationService::AllSources());
  }
  return location_bar->PageActionVisibleCount() == count;
}

bool ExtensionBrowserTest::WaitForExtensionHostsToLoad() {
  // Wait for all the extension hosts that exist to finish loading.
  content::NotificationRegistrar registrar;
  registrar.Add(this, chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING,
                content::NotificationService::AllSources());

  ExtensionProcessManager* manager =
        browser()->profile()->GetExtensionProcessManager();
  for (ExtensionProcessManager::const_iterator iter = manager->begin();
       iter != manager->end();) {
    if ((*iter)->did_stop_loading()) {
      ++iter;
    } else {
      ui_test_utils::RunMessageLoop();

      // Test activity may have modified the set of extension processes during
      // message processing, so re-start the iteration to catch added/removed
      // processes.
      iter = manager->begin();
    }
  }
  return true;
}

bool ExtensionBrowserTest::WaitForExtensionInstall() {
  int before = extension_installs_observed_;
  ui_test_utils::RegisterAndWait(this,
                                 chrome::NOTIFICATION_EXTENSION_INSTALLED,
                                 content::NotificationService::AllSources());
  return extension_installs_observed_ == (before + 1);
}

bool ExtensionBrowserTest::WaitForExtensionInstallError() {
  int before = extension_installs_observed_;
  ui_test_utils::RegisterAndWait(this,
                                 chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR,
                                 content::NotificationService::AllSources());
  return extension_installs_observed_ == before;
}

void ExtensionBrowserTest::WaitForExtensionLoad() {
  ui_test_utils::RegisterAndWait(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                                 content::NotificationService::AllSources());
  WaitForExtensionHostsToLoad();
}

bool ExtensionBrowserTest::WaitForExtensionCrash(
    const std::string& extension_id) {
  ExtensionService* service = browser()->profile()->GetExtensionService();

  if (!service->GetExtensionById(extension_id, true)) {
    // The extension is already unloaded, presumably due to a crash.
    return true;
  }
  ui_test_utils::RegisterAndWait(
      this, chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED,
      content::NotificationService::AllSources());
  return (service->GetExtensionById(extension_id, true) == NULL);
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
      MessageLoopForUI::current()->Quit();
      break;

    case chrome::NOTIFICATION_CRX_INSTALLER_DONE:
      VLOG(1) << "Got CRX_INSTALLER_DONE notification.";
      {
        const Extension* extension =
            content::Details<const Extension>(details).ptr();
        if (extension)
          last_loaded_extension_id_ = extension->id();
        else
          last_loaded_extension_id_ = "";
      }
      MessageLoopForUI::current()->Quit();
      break;

    case chrome::NOTIFICATION_EXTENSION_HOST_DID_STOP_LOADING:
      VLOG(1) << "Got EXTENSION_HOST_DID_STOP_LOADING notification.";
      MessageLoopForUI::current()->Quit();
      break;

    case chrome::NOTIFICATION_EXTENSION_INSTALLED:
      VLOG(1) << "Got EXTENSION_INSTALLED notification.";
      ++extension_installs_observed_;
      MessageLoopForUI::current()->Quit();
      break;

    case chrome::NOTIFICATION_EXTENSION_INSTALL_ERROR:
      VLOG(1) << "Got EXTENSION_INSTALL_ERROR notification.";
      MessageLoopForUI::current()->Quit();
      break;

    case chrome::NOTIFICATION_EXTENSION_PROCESS_TERMINATED:
      VLOG(1) << "Got EXTENSION_PROCESS_TERMINATED notification.";
      MessageLoopForUI::current()->Quit();
      break;

    case chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_COUNT_CHANGED: {
      LocationBarTesting* location_bar =
          browser()->window()->GetLocationBar()->GetLocationBarForTesting();
      VLOG(1) << "Got EXTENSION_PAGE_ACTION_COUNT_CHANGED notification. Number "
                 "of page actions: " << location_bar->PageActionCount();
      if (location_bar->PageActionCount() ==
          target_page_action_count_) {
        target_page_action_count_ = -1;
        MessageLoopForUI::current()->Quit();
      }
      break;
    }

    case chrome::NOTIFICATION_EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED: {
      LocationBarTesting* location_bar =
          browser()->window()->GetLocationBar()->GetLocationBarForTesting();
      VLOG(1) << "Got EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED notification. "
                 "Number of visible page actions: "
              << location_bar->PageActionVisibleCount();
      if (location_bar->PageActionVisibleCount() ==
          target_visible_page_action_count_) {
        target_visible_page_action_count_ = -1;
        MessageLoopForUI::current()->Quit();
      }
      break;
    }

    default:
      NOTREACHED();
      break;
  }
}
