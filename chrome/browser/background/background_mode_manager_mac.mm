// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/mac/mac_util.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/background/background_mode_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {
#if !defined(NDEBUG)
// The code to remove a login item has a potential race (because the code to
// set and check the kUserRemovedLoginItem pref runs on the UI thread, while
// the code that checks for a login item runs on the IO thread). We add this
// flag which should always match the value of the pref to see if we ever hit
// this race in practice.
static bool login_item_removed = false;
#endif

void SetUserRemovedLoginItemPrefCallback() {
  PrefService* service = g_browser_process->local_state();
  service->SetBoolean(prefs::kUserRemovedLoginItem, true);
}

void DisableLaunchOnStartupCallback() {
  // Check if Chrome is not a login Item, or is a Login Item but w/o 'hidden'
  // flag - most likely user has modified the setting, don't override it.
  bool is_hidden = false;
  if (!base::mac::CheckLoginItemStatus(&is_hidden)) {
    // No login item - this means the user must have already removed it, so
    // call back to the UI thread to set a preference so we don't try to
    // recreate it the next time they enable/install a background app.
#if !defined(NDEBUG)
    login_item_removed = true;
#endif
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(SetUserRemovedLoginItemPrefCallback));
    return;
  }

  // If the login item does not have the "hidden" flag set, just leave it there
  // since it means the user must have created it.
  if (!is_hidden)
    return;

  // Remove the login item we created.
  base::mac::RemoveFromLoginItems();
}

void SetUserCreatedLoginItemPrefCallback() {
  PrefService* service = g_browser_process->local_state();
  service->SetBoolean(prefs::kUserCreatedLoginItem, true);
}

void EnableLaunchOnStartupCallback(bool should_add_login_item) {
  // Check if Chrome is already a Login Item (avoid overriding user choice).
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

  if (should_add_login_item)
    base::mac::AddToLoginItems(true);  // Hide on startup.
#if !defined(NDEBUG)
  else
    DCHECK(!login_item_removed); // Check for race condition (see above).
#endif
}

}  // namespace

void BackgroundModeManager::EnableLaunchOnStartup(bool should_launch) {
  // This functionality is only defined for default profile, currently.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kUserDataDir))
    return;

  if (should_launch) {
    PrefService* service = g_browser_process->local_state();
    // Create a login item if the user did not remove our login item
    // previously. We call out to the FILE thread either way since we
    // want to check for a user-created login item.
    bool should_add_login_item =
        !service->GetBoolean(prefs::kUserRemovedLoginItem);
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            base::Bind(EnableLaunchOnStartupCallback,
                                       should_add_login_item));
  } else {
    PrefService* service = g_browser_process->local_state();
    if (service->GetBoolean(prefs::kUserCreatedLoginItem)) {
      // We didn't create the login item, so nothing to do here. Clear our
      // prefs so if the user removes the login item before installing a
      // background app, we will revert to the default behavior.
      service->ClearPref(prefs::kUserCreatedLoginItem);
      service->ClearPref(prefs::kUserRemovedLoginItem);
#if !defined(NDEBUG)
      login_item_removed = false;
#endif
      return;
    }
    // Call to the File thread to remove the login item since it requires
    // accessing the disk.
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE,
                            base::Bind(DisableLaunchOnStartupCallback));
  }
}

void BackgroundModeManager::DisplayAppInstalledNotification(
    const extensions::Extension* extension) {
  // TODO(atwilson): Display a platform-appropriate notification here.
  // http://crbug.com/74970
}

string16 BackgroundModeManager::GetPreferencesMenuLabel() {
  return l10n_util::GetStringUTF16(IDS_OPTIONS);
}
