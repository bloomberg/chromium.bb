// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_new_window_delegate.h"

#include "ash/content/keyboard_overlay/keyboard_overlay_view.h"
#include "ash/wm/window_util.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/extensions/api/terminal/terminal_extension_helper.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/ash/chrome_shell_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "chrome/common/url_constants.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "ui/base/window_open_disposition.h"

namespace {

void RestoreTabUsingProfile(Profile* profile) {
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile);
  service->RestoreMostRecentEntry(nullptr);
}

// Returns the browser for the active window, if any.
Browser* GetBrowserForActiveWindow() {
  return chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow());
}

}  // namespace

ChromeNewWindowDelegate::ChromeNewWindowDelegate() {}
ChromeNewWindowDelegate::~ChromeNewWindowDelegate() {}

// TabRestoreHelper is used to restore a tab. In particular when the user
// attempts to a restore a tab if the TabRestoreService hasn't finished loading
// this waits for it. Once the TabRestoreService finishes loading the tab is
// restored.
class ChromeNewWindowDelegate::TabRestoreHelper
    : public sessions::TabRestoreServiceObserver {
 public:
  TabRestoreHelper(ChromeNewWindowDelegate* delegate,
                   Profile* profile,
                   sessions::TabRestoreService* service)
      : delegate_(delegate), profile_(profile), tab_restore_service_(service) {
    tab_restore_service_->AddObserver(this);
  }

  ~TabRestoreHelper() override { tab_restore_service_->RemoveObserver(this); }

  sessions::TabRestoreService* tab_restore_service() {
    return tab_restore_service_;
  }

  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override {
  }

  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override {
    // This destroys us.
    delegate_->tab_restore_helper_.reset();
  }

  void TabRestoreServiceLoaded(sessions::TabRestoreService* service) override {
    RestoreTabUsingProfile(profile_);
    // This destroys us.
    delegate_->tab_restore_helper_.reset();
  }

 private:
  ChromeNewWindowDelegate* delegate_;
  Profile* profile_;
  sessions::TabRestoreService* tab_restore_service_;

  DISALLOW_COPY_AND_ASSIGN(TabRestoreHelper);
};

void ChromeNewWindowDelegate::NewTab() {
  Browser* browser = GetBrowserForActiveWindow();
  if (browser && browser->is_type_tabbed()) {
    chrome::NewTab(browser);
    return;
  }

  // Display a browser, setting the focus to the location bar after it is shown.
  {
    chrome::ScopedTabbedBrowserDisplayer displayer(
        ProfileManager::GetActiveUserProfile());
    browser = displayer.browser();
    chrome::NewTab(browser);
  }

  browser->SetFocusToLocationBar(false);
}

void ChromeNewWindowDelegate::NewWindow(bool is_incognito) {
  Browser* browser = GetBrowserForActiveWindow();
  Profile* profile = (browser && browser->profile())
                         ? browser->profile()->GetOriginalProfile()
                         : ProfileManager::GetActiveUserProfile();
  chrome::NewEmptyWindow(is_incognito ? profile->GetOffTheRecordProfile()
                                      : profile);
}

void ChromeNewWindowDelegate::OpenFileManager() {
  using file_manager::kFileManagerAppId;
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  const ExtensionService* const service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  if (!service ||
      !extensions::util::IsAppLaunchableWithoutEnabling(kFileManagerAppId,
                                                        profile)) {
    return;
  }

  const extensions::Extension* const extension =
      service->GetInstalledExtension(kFileManagerAppId);
  OpenApplication(CreateAppLaunchParamsUserContainer(
      profile, extension, NEW_FOREGROUND_TAB, extensions::SOURCE_KEYBOARD));
}

void ChromeNewWindowDelegate::OpenCrosh() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  GURL crosh_url =
      extensions::TerminalExtensionHelper::GetCroshExtensionURL(profile);
  if (!crosh_url.is_valid())
    return;
  chrome::ScopedTabbedBrowserDisplayer displayer(profile);
  Browser* browser = displayer.browser();
  content::WebContents* page = browser->OpenURL(
      content::OpenURLParams(crosh_url, content::Referrer(), NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_GENERATED, false));
  browser->window()->Show();
  browser->window()->Activate();
  page->Focus();
}

void ChromeNewWindowDelegate::OpenGetHelp() {
  Profile* const profile = ProfileManager::GetActiveUserProfile();
  chrome::ShowHelpForProfile(profile, chrome::HELP_SOURCE_KEYBOARD);
}

void ChromeNewWindowDelegate::RestoreTab() {
  if (tab_restore_helper_.get()) {
    DCHECK(!tab_restore_helper_->tab_restore_service()->IsLoaded());
    return;
  }

  Browser* browser = GetBrowserForActiveWindow();
  Profile* profile = browser ? browser->profile() : NULL;
  if (!profile)
    profile = ProfileManager::GetActiveUserProfile();
  if (profile->IsOffTheRecord())
    return;
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(profile);
  if (!service)
    return;

  if (service->IsLoaded()) {
    RestoreTabUsingProfile(profile);
  } else {
    tab_restore_helper_.reset(new TabRestoreHelper(this, profile, service));
    service->LoadTabsFromLastSession();
  }
}

void ChromeNewWindowDelegate::ShowKeyboardOverlay() {
  // TODO(mazda): Move the show logic to ash (http://crbug.com/124222).
  Profile* profile = ProfileManager::GetActiveUserProfile();
  std::string url(chrome::kChromeUIKeyboardOverlayURL);
  ash::KeyboardOverlayView::ShowDialog(profile, new ChromeWebContentsHandler,
                                       GURL(url));
}

void ChromeNewWindowDelegate::ShowTaskManager() {
  chrome::OpenTaskManager(NULL);
}

void ChromeNewWindowDelegate::OpenFeedbackPage() {
  chrome::OpenFeedbackDialog(GetBrowserForActiveWindow());
}
