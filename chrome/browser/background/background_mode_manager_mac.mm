// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {

void DisableLaunchOnStartupCallback() {
  // Check if Chrome is not a login Item, or is a Login Item but w/o 'hidden'
  // flag - most likely user has modified the setting, don't override it.
  bool is_hidden = false;
  if (!base::mac::CheckLoginItemStatus(&is_hidden) || !is_hidden)
    return;

  base::mac::RemoveFromLoginItems();
}

void SetUserCreatedLoginItemPrefCallback() {
  PrefService* service = g_browser_process->local_state();
  service->SetBoolean(prefs::kUserCreatedLoginItem, true);
}

void EnableLaunchOnStartupCallback() {
  // Return if Chrome is already a Login Item (avoid overriding user choice).
  if (base::mac::CheckLoginItemStatus(NULL)) {
    // Call back to the UI thread to set our preference so we don't delete the
    // user's login item when we disable launch on startup. There's a race
    // condition here if the user disables launch on startup before our callback
    // is run, but the user can manually disable "Open At Login" via the dock if
    // this happens.
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(SetUserCreatedLoginItemPrefCallback));
    return;
  }

  base::mac::AddToLoginItems(true);  // Hide on startup.
}

}  // namespace

void BackgroundModeManager::EnableLaunchOnStartup(bool should_launch) {
  // This functionality is only defined for default profile, currently.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUserDataDir))
    return;

  if (should_launch) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            base::Bind(EnableLaunchOnStartupCallback));
  } else {
    PrefService* service = g_browser_process->local_state();
    if (service->GetBoolean(prefs::kUserCreatedLoginItem)) {
      // We didn't create the login item, so nothing to do here.
      service->ClearPref(prefs::kUserCreatedLoginItem);
      return;
    }
    // Call to the File thread to remove the login item since it requires
    // accessing the disk.
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            base::Bind(DisableLaunchOnStartupCallback));
  }
}

void BackgroundModeManager::DisplayAppInstalledNotification(
    const Extension* extension) {
  // TODO(atwilson): Display a platform-appropriate notification here.
  // http://crbug.com/74970
}

string16 BackgroundModeManager::GetPreferencesMenuLabel() {
  return l10n_util::GetStringUTF16(IDS_OPTIONS);
}
