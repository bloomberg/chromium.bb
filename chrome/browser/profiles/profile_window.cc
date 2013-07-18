// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_window.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/user_metrics.h"

#if !defined(OS_IOS)
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#endif  // !defined (OS_IOS)

using content::UserMetricsAction;

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

}  // namespace profiles
