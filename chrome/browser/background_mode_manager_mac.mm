// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/mac_util.h"
#include "chrome/browser/background_mode_manager.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/common/app_mode_common_mac.h"
#include "chrome/common/chrome_switches.h"
#include "grit/generated_resources.h"

namespace {

class DisableLaunchOnStartupTask : public Task {
 public:
  virtual void Run();
};

class EnableLaunchOnStartupTask : public Task {
 public:
  virtual void Run();
};

const CFStringRef kLaunchOnStartupResetAllowedPrefsKey =
    CFSTR("LaunchOnStartupResetAllowed");

void DisableLaunchOnStartupTask::Run() {
  Boolean key_exists_and_has_valid_format;  // ignored
  if (!CFPreferencesGetAppBooleanValue(kLaunchOnStartupResetAllowedPrefsKey,
                                       app_mode::kAppPrefsID,
                                       &key_exists_and_has_valid_format))
    return;

  CFPreferencesSetAppValue(kLaunchOnStartupResetAllowedPrefsKey,
                           kCFBooleanFalse,
                           app_mode::kAppPrefsID);

  // Check if Chrome is not a login Item, or is a Login Item but w/o 'hidden'
  // flag - most likely user has modified the setting, don't override it.
  bool is_hidden = false;
  if (!mac_util::CheckLoginItemStatus(&is_hidden) || !is_hidden)
    return;

  mac_util::RemoveFromLoginItems();
}

void EnableLaunchOnStartupTask::Run() {
  // Return if Chrome is already a Login Item (avoid overriding user choice).
  if (mac_util::CheckLoginItemStatus(NULL))
    return;

  mac_util::AddToLoginItems(true);  // Hide on startup.
  CFPreferencesSetAppValue(kLaunchOnStartupResetAllowedPrefsKey,
                           kCFBooleanTrue,
                           app_mode::kAppPrefsID);
}

}  // namespace

void BackgroundModeManager::EnableLaunchOnStartup(bool should_launch) {
  // This functionality is only defined for default profile, currently.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUserDataDir))
    return;

  if (should_launch) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            new EnableLaunchOnStartupTask());
  } else {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            new DisableLaunchOnStartupTask());
  }
}

string16 BackgroundModeManager::GetPreferencesMenuLabel() {
  return l10n_util::GetStringUTF16(IDS_OPTIONS);
}
