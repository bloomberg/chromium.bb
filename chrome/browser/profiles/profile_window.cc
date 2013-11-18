// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_window.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"

#if !defined(OS_IOS)
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#endif  // !defined (OS_IOS)

using content::BrowserThread;
using content::UserMetricsAction;

namespace {

// Handles running a callback when a new Browser has been completely created.
class BrowserAddedObserver : public chrome::BrowserListObserver {
 public:
  explicit BrowserAddedObserver(
      profiles::ProfileSwitchingDoneCallback callback) : callback_(callback) {
    BrowserList::AddObserver(this);
  }
  virtual ~BrowserAddedObserver() {
    BrowserList::RemoveObserver(this);
  }

 private:
  // Overridden from BrowserListObserver:
  virtual void OnBrowserAdded(Browser* browser) OVERRIDE {
    DCHECK(!callback_.is_null());
    callback_.Run();
  }

  profiles::ProfileSwitchingDoneCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAddedObserver);
};

void OpenBrowserWindowForProfile(
    profiles::ProfileSwitchingDoneCallback callback,
    bool always_create,
    bool is_new_profile,
    chrome::HostDesktopType desktop_type,
    Profile* profile,
    Profile::CreateStatus status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;

  chrome::startup::IsProcessStartup is_process_startup =
      chrome::startup::IS_NOT_PROCESS_STARTUP;
  chrome::startup::IsFirstRun is_first_run = chrome::startup::IS_NOT_FIRST_RUN;

  // If this is a brand new profile, then start a first run window.
  if (is_new_profile) {
    is_process_startup = chrome::startup::IS_PROCESS_STARTUP;
    is_first_run = chrome::startup::IS_FIRST_RUN;
  }

  // If we are not showing any browsers windows (we could be showing the
  // desktop User Manager, for example), then this is a process startup browser.
  if (BrowserList::GetInstance(desktop_type)->size() == 0)
    is_process_startup = chrome::startup::IS_PROCESS_STARTUP;

  // If there is a callback, create an observer to make sure it is only
  // run when the browser has been completely created.
  scoped_ptr<BrowserAddedObserver> browser_added_observer;
  if (!callback.is_null())
    browser_added_observer.reset(new BrowserAddedObserver(callback));

  profiles::FindOrCreateNewWindowForProfile(
      profile,
      is_process_startup,
      is_first_run,
      desktop_type,
      always_create);
}

}  // namespace

namespace profiles {

void FindOrCreateNewWindowForProfile(
    Profile* profile,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run,
    chrome::HostDesktopType desktop_type,
    bool always_create) {
#if defined(OS_IOS)
  NOTREACHED();
#else
  DCHECK(profile);

  if (!always_create) {
    Browser* browser = chrome::FindTabbedBrowser(profile, false, desktop_type);
    if (browser) {
      browser->window()->Activate();
      return;
    }
  }

  content::RecordAction(UserMetricsAction("NewWindow"));
  CommandLine command_line(CommandLine::NO_PROGRAM);
  int return_code;
  StartupBrowserCreator browser_creator;
  browser_creator.LaunchBrowser(command_line, profile, base::FilePath(),
                                process_startup, is_first_run, &return_code);
#endif  // defined(OS_IOS)
}

void SwitchToProfile(
    const base::FilePath& path,
    chrome::HostDesktopType desktop_type,
    bool always_create,
    ProfileSwitchingDoneCallback callback) {
  g_browser_process->profile_manager()->CreateProfileAsync(
      path,
      base::Bind(&OpenBrowserWindowForProfile,
                 callback,
                 always_create,
                 false,
                 desktop_type),
      string16(),
      string16(),
      std::string());
}

void SwitchToGuestProfile(chrome::HostDesktopType desktop_type,
                          ProfileSwitchingDoneCallback callback) {
  g_browser_process->profile_manager()->CreateProfileAsync(
      ProfileManager::GetGuestProfilePath(),
      base::Bind(&OpenBrowserWindowForProfile,
                 callback,
                 false,
                 false,
                 desktop_type),
      string16(),
      string16(),
      std::string());
}

void CreateAndSwitchToNewProfile(chrome::HostDesktopType desktop_type,
                                 ProfileSwitchingDoneCallback callback) {
  ProfileManager::CreateMultiProfileAsync(
      string16(),
      string16(),
      base::Bind(&OpenBrowserWindowForProfile,
                 callback,
                 true,
                 true,
                 desktop_type),
      std::string());
}

void CloseGuestProfileWindows() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetProfileByPath(
      ProfileManager::GetGuestProfilePath());

  if (profile) {
    BrowserList::CloseAllBrowsersWithProfile(profile);
  }
}

}  // namespace profiles
