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
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#endif  // !defined (OS_IOS)

using content::BrowserThread;
using content::UserMetricsAction;

namespace {

void OpenBrowserWindowForProfile(bool always_create,
                                 chrome::HostDesktopType desktop_type,
                                 Profile* profile,
                                 Profile::CreateStatus status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (status == Profile::CREATE_STATUS_INITIALIZED) {
    profiles::FindOrCreateNewWindowForProfile(
        profile,
        chrome::startup::IS_NOT_PROCESS_STARTUP,
        chrome::startup::IS_NOT_FIRST_RUN,
        desktop_type,
        always_create);
  }
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
    bool always_create) {
  g_browser_process->profile_manager()->CreateProfileAsync(
      path,
      base::Bind(&OpenBrowserWindowForProfile,
                 always_create,
                 desktop_type),
      string16(),
      string16(),
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
