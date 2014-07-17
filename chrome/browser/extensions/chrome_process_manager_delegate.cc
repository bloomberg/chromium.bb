// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_process_manager_delegate.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/one_shot_event.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_switches.h"
#endif

namespace extensions {

ChromeProcessManagerDelegate::ChromeProcessManagerDelegate() {
  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_WINDOW_READY,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 chrome::NOTIFICATION_PROFILE_CREATED,
                 content::NotificationService::AllSources());
  // TODO(jamescook): Move observation of NOTIFICATION_PROFILE_DESTROYED here.
  // http://crbug.com/392658
}

ChromeProcessManagerDelegate::~ChromeProcessManagerDelegate() {
}

bool ChromeProcessManagerDelegate::IsBackgroundPageAllowed(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  // Disallow if the current session is a Guest mode session but the current
  // browser context is *not* off-the-record. Such context is artificial and
  // background page shouldn't be created in it.
  // http://crbug.com/329498
  return !(profile->IsGuestSession() && !profile->IsOffTheRecord());
}

bool ChromeProcessManagerDelegate::DeferCreatingStartupBackgroundHosts(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);

  // The profile may not be valid yet if it is still being initialized.
  // In that case, defer loading, since it depends on an initialized profile.
  // Background hosts will be loaded later via NOTIFICATION_PROFILE_CREATED.
  // http://crbug.com/222473
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return true;

#if defined(OS_ANDROID)
  return false;
#else
  // There are no browser windows open and the browser process was
  // started to show the app launcher. Background hosts will be loaded later
  // via NOTIFICATION_BROWSER_WINDOW_READY. http://crbug.com/178260
  return chrome::GetTotalBrowserCountForProfile(profile) == 0 &&
         CommandLine::ForCurrentProcess()->HasSwitch(switches::kShowAppList);
#endif
}

void ChromeProcessManagerDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_BROWSER_WINDOW_READY: {
      Browser* browser = content::Source<Browser>(source).ptr();
      OnBrowserWindowReady(browser);
      break;
    }
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      Profile* profile = content::Source<Profile>(source).ptr();
      OnProfileCreated(profile);
      break;
    }

    default:
      NOTREACHED();
  }
}

void ChromeProcessManagerDelegate::OnBrowserWindowReady(Browser* browser) {
  Profile* profile = browser->profile();
  DCHECK(profile);

  // If the extension system isn't ready yet the background hosts will be
  // created automatically when it is.
  ExtensionSystem* system = ExtensionSystem::Get(profile);
  if (!system->ready().is_signaled())
    return;

  // Inform the process manager for this profile that the window is ready.
  // We continue to observe the notification in case browser windows open for
  // a related incognito profile or other regular profiles.
  ProcessManager* manager = system->process_manager();
  if (!manager)  // Tests may not have a process manager.
    return;
  DCHECK_EQ(profile, manager->GetBrowserContext());
  manager->MaybeCreateStartupBackgroundHosts();

  // For incognito profiles also inform the original profile's process manager
  // that the window is ready. This will usually be a no-op because the
  // original profile's process manager should have been informed when the
  // non-incognito window opened.
  if (profile->IsOffTheRecord()) {
    Profile* original_profile = profile->GetOriginalProfile();
    ProcessManager* original_manager =
        ExtensionSystem::Get(original_profile)->process_manager();
    DCHECK(original_manager);
    DCHECK_EQ(original_profile, original_manager->GetBrowserContext());
    original_manager->MaybeCreateStartupBackgroundHosts();
  }
}

void ChromeProcessManagerDelegate::OnProfileCreated(Profile* profile) {
  // Incognito profiles are handled by their original profile.
  if (profile->IsOffTheRecord())
    return;

  // The profile can be created before the extension system is ready.
  ProcessManager* manager = ExtensionSystem::Get(profile)->process_manager();
  if (!manager)
    return;

  // The profile might have been initialized asynchronously (in parallel with
  // extension system startup). Now that initialization is complete the
  // ProcessManager can load deferred background pages.
  manager->MaybeCreateStartupBackgroundHosts();
}

}  // namespace extensions
