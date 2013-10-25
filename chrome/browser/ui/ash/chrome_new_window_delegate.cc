// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_new_window_delegate.h"

#include "ash/wm/window_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_observer.h"
#include "chrome/browser/ui/ash/chrome_shell_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"

namespace {

void RestoreTabUsingProfile(Profile* profile) {
  TabRestoreService* service = TabRestoreServiceFactory::GetForProfile(profile);
  service->RestoreMostRecentEntry(NULL, chrome::HOST_DESKTOP_TYPE_ASH);
}

}  // namespace

ChromeNewWindowDelegate::ChromeNewWindowDelegate() {}
ChromeNewWindowDelegate::~ChromeNewWindowDelegate() {}

// TabRestoreHelper is used to restore a tab. In particular when the user
// attempts to a restore a tab if the TabRestoreService hasn't finished loading
// this waits for it. Once the TabRestoreService finishes loading the tab is
// restored.
class ChromeNewWindowDelegate::TabRestoreHelper
    : public TabRestoreServiceObserver {
 public:
  TabRestoreHelper(ChromeNewWindowDelegate* delegate,
                   Profile* profile,
                   TabRestoreService* service)
      : delegate_(delegate),
        profile_(profile),
        tab_restore_service_(service) {
    tab_restore_service_->AddObserver(this);
  }

  virtual ~TabRestoreHelper() {
    tab_restore_service_->RemoveObserver(this);
  }

  TabRestoreService* tab_restore_service() { return tab_restore_service_; }

  virtual void TabRestoreServiceChanged(TabRestoreService* service) OVERRIDE {
  }

  virtual void TabRestoreServiceDestroyed(TabRestoreService* service) OVERRIDE {
    // This destroys us.
    delegate_->tab_restore_helper_.reset();
  }

  virtual void TabRestoreServiceLoaded(TabRestoreService* service) OVERRIDE {
    RestoreTabUsingProfile(profile_);
    // This destroys us.
    delegate_->tab_restore_helper_.reset();
  }

 private:
  ChromeNewWindowDelegate* delegate_;
  Profile* profile_;
  TabRestoreService* tab_restore_service_;

  DISALLOW_COPY_AND_ASSIGN(TabRestoreHelper);
};

void ChromeNewWindowDelegate::NewTab() {
  Browser* browser = GetTargetBrowser();

  // If the browser was not active, we call BrowserWindow::Show to make it
  // visible. Otherwise, we let Browser::NewTab handle the active window change.
  const bool was_active = browser->window()->IsActive();
  chrome::NewTab(browser);
  if (!was_active)
    browser->window()->Show();
}

void ChromeNewWindowDelegate::NewWindow(bool is_incognito) {
  Profile* profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  chrome::NewEmptyWindow(
      is_incognito ? profile->GetOffTheRecordProfile() : profile,
      chrome::HOST_DESKTOP_TYPE_ASH);
}

void ChromeNewWindowDelegate::RestoreTab() {
  if (tab_restore_helper_.get()) {
    DCHECK(!tab_restore_helper_->tab_restore_service()->IsLoaded());
    return;
  }

  Browser* browser = chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow());
  Profile* profile = browser ? browser->profile() : NULL;
  if (!profile)
    profile = ProfileManager::GetDefaultProfileOrOffTheRecord();
  if (profile->IsOffTheRecord())
    return;
  TabRestoreService* service =
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

void ChromeNewWindowDelegate::ShowTaskManager() {
  chrome::OpenTaskManager(NULL);
}

void ChromeNewWindowDelegate::OpenFeedbackPage() {
  chrome::OpenFeedbackDialog(GetTargetBrowserIfAvailable());
}

// static
Browser* ChromeNewWindowDelegate::GetTargetBrowser() {
  Browser* browser = GetTargetBrowserIfAvailable();
  if (browser)
    return browser;
  return chrome::FindOrCreateTabbedBrowser(
      ProfileManager::GetDefaultProfileOrOffTheRecord(),
      chrome::HOST_DESKTOP_TYPE_ASH);
}

// static
Browser* ChromeNewWindowDelegate::GetTargetBrowserIfAvailable() {
  return chrome::FindBrowserWithWindow(ash::wm::GetActiveWindow());
}
