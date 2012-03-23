// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"

// static
void ManagedMode::RegisterPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kInManagedMode, false);
  // Set the value directly in the PrefService instead of using
  // CommandLinePrefStore so we can change it at runtime.
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kManaged))
    SetInManagedMode(true);
}

// static
bool ManagedMode::IsInManagedMode() {
  // |g_browser_process| can be NULL during startup.
  if (!g_browser_process)
    return false;
  return g_browser_process->local_state()->GetBoolean(prefs::kInManagedMode);
}

// static
bool ManagedMode::EnterManagedMode(Profile* profile) {
  bool confirmed = PlatformConfirmEnter();
  if (confirmed) {
    // Close all other profiles.
    // TODO(bauerb): This may not close all windows, for example if there is an
    // onbeforeunload handler. We should cancel entering managed mode in that
    // case.
    std::vector<Profile*> profiles =
        g_browser_process->profile_manager()->GetLoadedProfiles();
    for (std::vector<Profile*>::iterator it = profiles.begin();
         it != profiles.end(); ++it) {
      if (*it != profile)
        BrowserList::CloseAllBrowsersWithProfile(*it);
    }

    SetInManagedMode(true);
  }
  return confirmed;
}

// static
void ManagedMode::LeaveManagedMode() {
  bool confirmed = PlatformConfirmLeave();
  if (confirmed)
    SetInManagedMode(false);
}

// static
bool ManagedMode::PlatformConfirmEnter() {
  // TODO(bauerb): Show platform-specific confirmation dialog.
  return true;
}

// static
bool ManagedMode::PlatformConfirmLeave() {
  // TODO(bauerb): Show platform-specific confirmation dialog.
  return true;
}

// static
void ManagedMode::SetInManagedMode(bool in_managed_mode) {
  g_browser_process->local_state()->SetBoolean(prefs::kInManagedMode,
                                               in_managed_mode);
  // This causes the avatar and the profile menu to get updated.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_PROFILE_CACHED_INFO_CHANGED,
      content::NotificationService::AllBrowserContextsAndSources(),
      content::NotificationService::NoDetails());
}

