// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_notification_observer.h"

#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension_messages.h"

namespace extensions {

ChromeNotificationObserver::ChromeNotificationObserver() {
  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_WINDOW_READY,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());
}

ChromeNotificationObserver::~ChromeNotificationObserver() {}

void ChromeNotificationObserver::OnBrowserWindowReady(Browser* browser) {
  Profile* profile = browser->profile();
  DCHECK(profile);

  // Inform the process manager for this profile that the window is ready.
  // We continue to observe the notification in case browser windows open for
  // a related incognito profile or other regular profiles.
  extensions::ProcessManager* manager =
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
    extensions::ProcessManager* original_manager =
        ExtensionSystem::Get(original_profile)->process_manager();
    DCHECK(original_manager);
    DCHECK_EQ(original_profile, original_manager->GetBrowserContext());
    original_manager->OnBrowserWindowReady();
  }
}

void ChromeNotificationObserver::OnRendererProcessCreated(
    content::RenderProcessHost* process) {
  // Extensions need to know the channel for API restrictions. Send the channel
  // to all renderers, as the non-extension renderers may have content scripts.
  process->Send(new ExtensionMsg_SetChannel(GetCurrentChannel()));
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

    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      content::RenderProcessHost* process =
          content::Source<content::RenderProcessHost>(source).ptr();
      OnRendererProcessCreated(process);
      break;
    }

    default:
      NOTREACHED();
  }
}

}  // namespace extensions
