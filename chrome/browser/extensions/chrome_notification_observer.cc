// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_notification_observer.h"

#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/notification_service.h"

namespace extensions {

ChromeNotificationObserver::ChromeNotificationObserver() {
  // Notifications for ExtensionProcessManager
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_WINDOW_READY,
                 content::NotificationService::AllSources());
}

ChromeNotificationObserver::~ChromeNotificationObserver() {}

void ChromeNotificationObserver::OnBrowserWindowReady(Browser* browser) {
  Profile* profile = browser->profile();
  DCHECK(profile);

  // Inform the process manager for this profile that the window is ready.
  // We continue to observe the notification in case browser windows open for
  // a related incognito profile or other regular profiles.
  ExtensionProcessManager* manager =
      ExtensionSystem::Get(profile)->process_manager();
  if (!manager)  // Tests may not have a process manager.
    return;
  DCHECK_EQ(profile, manager->GetBrowserContext());
  manager->OnBrowserWindowReady();

  // For incognito profiles also inform the original profile's process manager
  // that the window is ready. This will usually be a no-op because the
  // original profile's process manager should have been informed when the
  // non-incognito window opened.
  if (profile->IsOffTheRecord()) {
    Profile* original_profile = profile->GetOriginalProfile();
    ExtensionProcessManager* original_manager =
        ExtensionSystem::Get(original_profile)->process_manager();
    DCHECK(original_manager);
    DCHECK_EQ(original_profile, original_manager->GetBrowserContext());
    original_manager->OnBrowserWindowReady();
  }
}

void ChromeNotificationObserver::Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_BROWSER_WINDOW_READY: {
      Browser* browser = content::Source<Browser>(source).ptr();
      OnBrowserWindowReady(browser);
      break;
    }

    default:
      NOTREACHED();
  }
}

}  // namespace extensions
