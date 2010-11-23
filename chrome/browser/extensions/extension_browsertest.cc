// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"

#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/ui_test_utils.h"

ExtensionBrowserTest::ExtensionBrowserTest()
    : loaded_(false),
      installed_(false),
      extension_installs_observed_(0),
      target_page_action_count_(-1),
      target_visible_page_action_count_(-1) {
}

void ExtensionBrowserTest::SetUpCommandLine(CommandLine* command_line) {
  // This enables DOM automation for tab contentses.
  EnableDOMAutomation();

  // This enables it for extension hosts.
  ExtensionHost::EnableDOMAutomation();

  PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_);
  test_data_dir_ = test_data_dir_.AppendASCII("extensions");

#if defined(OS_CHROMEOS)
  // This makes sure that we create the Default profile first, with no
  // ExtensionsService and then the real profile with one, as we do when
  // running on chromeos.
  command_line->AppendSwitchASCII(switches::kLoginUser,
                                  "TestUser@gmail.com");
  command_line->AppendSwitchASCII(switches::kLoginProfile, "user");
  command_line->AppendSwitch(switches::kNoFirstRun);
#endif
}

bool ExtensionBrowserTest::LoadExtensionImpl(const FilePath& path,
                                             bool incognito_enabled) {
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  {
    NotificationRegistrar registrar;
    registrar.Add(this, NotificationType::EXTENSION_LOADED,
                  NotificationService::AllSources());
    service->LoadExtension(path);
    ui_test_utils::RunMessageLoop();
  }

  // Find the extension by iterating backwards since it is likely last.
  FilePath extension_path = path;
  file_util::AbsolutePath(&extension_path);
  const Extension* extension = NULL;
  for (ExtensionList::const_reverse_iterator iter =
           service->extensions()->rbegin();
       iter != service->extensions()->rend(); ++iter) {
    if ((*iter)->path() == extension_path) {
      extension = *iter;
      break;
    }
  }
  if (!extension)
    return false;

  if (incognito_enabled) {
    // Enable the incognito bit in the extension prefs. The call to
    // OnExtensionInstalled ensures the other extension prefs are set up with
    // the defaults.
    service->extension_prefs()->OnExtensionInstalled(
        extension, Extension::ENABLED, false);
    service->SetIsIncognitoEnabled(extension, true);
  }

  return WaitForExtensionHostsToLoad();
}

bool ExtensionBrowserTest::LoadExtension(const FilePath& path) {
  return LoadExtensionImpl(path, false);
}

bool ExtensionBrowserTest::LoadExtensionIncognito(const FilePath& path) {
  return LoadExtensionImpl(path, true);
}

// This class is used to simulate an installation abort by the user.
class MockAbortExtensionInstallUI : public ExtensionInstallUI {
 public:
  MockAbortExtensionInstallUI() : ExtensionInstallUI(NULL) {}

  // Simulate a user abort on an extension installation.
  virtual void ConfirmInstall(Delegate* delegate, const Extension* extension) {
    delegate->InstallUIAbort();
    MessageLoopForUI::current()->Quit();
  }

  virtual void ConfirmUninstall(Delegate* delegate,
                                const Extension* extension) {}

  virtual void OnInstallSuccess(const Extension* extension) {}

  virtual void OnInstallFailure(const std::string& error) {}
};

bool ExtensionBrowserTest::InstallOrUpdateExtension(const std::string& id,
                                                    const FilePath& path,
                                                    InstallUIType ui_type,
                                                    int expected_change) {
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  service->set_show_extensions_prompts(false);
  size_t num_before = service->extensions()->size();

  {
    NotificationRegistrar registrar;
    registrar.Add(this, NotificationType::EXTENSION_LOADED,
                  NotificationService::AllSources());
    registrar.Add(this, NotificationType::EXTENSION_UPDATE_DISABLED,
                  NotificationService::AllSources());
    registrar.Add(this, NotificationType::EXTENSION_INSTALL_ERROR,
                  NotificationService::AllSources());

    ExtensionInstallUI* install_ui = NULL;
    if (ui_type == INSTALL_UI_TYPE_CANCEL)
      install_ui = new MockAbortExtensionInstallUI();
    else if (ui_type == INSTALL_UI_TYPE_NORMAL)
      install_ui = new ExtensionInstallUI(browser()->profile());

    scoped_refptr<CrxInstaller> installer(
        new CrxInstaller(service, install_ui));
    installer->set_expected_id(id);
    installer->InstallCrx(path);

    ui_test_utils::RunMessageLoop();
  }

  size_t num_after = service->extensions()->size();
  if (num_after != (num_before + expected_change)) {
    VLOG(1) << "Num extensions before: " << base::IntToString(num_before)
            << " num after: " << base::IntToString(num_after)
            << " Installed extensions follow:";

    for (size_t i = 0; i < service->extensions()->size(); ++i)
      VLOG(1) << "  " << (*service->extensions())[i]->id();

    VLOG(1) << "Errors follow:";
    const std::vector<std::string>* errors =
        ExtensionErrorReporter::GetInstance()->GetErrors();
    for (std::vector<std::string>::const_iterator iter = errors->begin();
         iter != errors->end(); ++iter)
      VLOG(1) << *iter;

    return false;
  }

  return WaitForExtensionHostsToLoad();
}

void ExtensionBrowserTest::ReloadExtension(const std::string& extension_id) {
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  service->ReloadExtension(extension_id);
  ui_test_utils::RegisterAndWait(this,
                                 NotificationType::EXTENSION_LOADED,
                                 NotificationService::AllSources());
}

void ExtensionBrowserTest::UnloadExtension(const std::string& extension_id) {
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  service->UnloadExtension(extension_id);
}

void ExtensionBrowserTest::UninstallExtension(const std::string& extension_id) {
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  service->UninstallExtension(extension_id, false);
}

void ExtensionBrowserTest::DisableExtension(const std::string& extension_id) {
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  service->DisableExtension(extension_id);
}

void ExtensionBrowserTest::EnableExtension(const std::string& extension_id) {
  ExtensionsService* service = browser()->profile()->GetExtensionsService();
  service->EnableExtension(extension_id);
}

bool ExtensionBrowserTest::WaitForPageActionCountChangeTo(int count) {
  LocationBarTesting* location_bar =
      browser()->window()->GetLocationBar()->GetLocationBarForTesting();
  if (location_bar->PageActionCount() != count) {
    target_page_action_count_ = count;
    ui_test_utils::RegisterAndWait(this,
        NotificationType::EXTENSION_PAGE_ACTION_COUNT_CHANGED,
        NotificationService::AllSources());
  }
  return location_bar->PageActionCount() == count;
}

bool ExtensionBrowserTest::WaitForPageActionVisibilityChangeTo(int count) {
  LocationBarTesting* location_bar =
      browser()->window()->GetLocationBar()->GetLocationBarForTesting();
  if (location_bar->PageActionVisibleCount() != count) {
    target_visible_page_action_count_ = count;
    ui_test_utils::RegisterAndWait(this,
        NotificationType::EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED,
        NotificationService::AllSources());
  }
  return location_bar->PageActionVisibleCount() == count;
}

bool ExtensionBrowserTest::WaitForExtensionHostsToLoad() {
  // Wait for all the extension hosts that exist to finish loading.
  NotificationRegistrar registrar;
  registrar.Add(this, NotificationType::EXTENSION_HOST_DID_STOP_LOADING,
                NotificationService::AllSources());

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
  ui_test_utils::RegisterAndWait(this, NotificationType::EXTENSION_INSTALLED,
                                 NotificationService::AllSources());
  return extension_installs_observed_ == (before + 1);
}

bool ExtensionBrowserTest::WaitForExtensionInstallError() {
  int before = extension_installs_observed_;
  ui_test_utils::RegisterAndWait(this,
                                 NotificationType::EXTENSION_INSTALL_ERROR,
                                 NotificationService::AllSources());
  return extension_installs_observed_ == before;
}

void ExtensionBrowserTest::WaitForExtensionLoad() {
  ui_test_utils::RegisterAndWait(this, NotificationType::EXTENSION_LOADED,
                                 NotificationService::AllSources());
  WaitForExtensionHostsToLoad();
}

bool ExtensionBrowserTest::WaitForExtensionCrash(
    const std::string& extension_id) {
  ExtensionsService* service = browser()->profile()->GetExtensionsService();

  if (!service->GetExtensionById(extension_id, true)) {
    // The extension is already unloaded, presumably due to a crash.
    return true;
  }
  ui_test_utils::RegisterAndWait(this,
                                 NotificationType::EXTENSION_PROCESS_TERMINATED,
                                 NotificationService::AllSources());
  return (service->GetExtensionById(extension_id, true) == NULL);
}

void ExtensionBrowserTest::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  switch (type.value) {
    case NotificationType::EXTENSION_LOADED:
      last_loaded_extension_id_ = Details<const Extension>(details).ptr()->id();
      VLOG(1) << "Got EXTENSION_LOADED notification.";
      MessageLoopForUI::current()->Quit();
      break;

    case NotificationType::EXTENSION_UPDATE_DISABLED:
      VLOG(1) << "Got EXTENSION_UPDATE_DISABLED notification.";
      MessageLoopForUI::current()->Quit();
      break;

    case NotificationType::EXTENSION_HOST_DID_STOP_LOADING:
      VLOG(1) << "Got EXTENSION_HOST_DID_STOP_LOADING notification.";
      MessageLoopForUI::current()->Quit();
      break;

    case NotificationType::EXTENSION_INSTALLED:
      VLOG(1) << "Got EXTENSION_INSTALLED notification.";
      ++extension_installs_observed_;
      MessageLoopForUI::current()->Quit();
      break;

    case NotificationType::EXTENSION_INSTALL_ERROR:
      VLOG(1) << "Got EXTENSION_INSTALL_ERROR notification.";
      MessageLoopForUI::current()->Quit();
      break;

    case NotificationType::EXTENSION_PROCESS_CREATED:
      VLOG(1) << "Got EXTENSION_PROCESS_CREATED notification.";
      MessageLoopForUI::current()->Quit();
      break;

    case NotificationType::EXTENSION_PROCESS_TERMINATED:
      VLOG(1) << "Got EXTENSION_PROCESS_TERMINATED notification.";
      MessageLoopForUI::current()->Quit();
      break;

    case NotificationType::EXTENSION_PAGE_ACTION_COUNT_CHANGED: {
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

    case NotificationType::EXTENSION_PAGE_ACTION_VISIBILITY_CHANGED: {
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
