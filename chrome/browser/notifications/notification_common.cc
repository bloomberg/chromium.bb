// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_common.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_context.h"

// static
void NotificationCommon::OpenNotificationSettings(
    content::BrowserContext* browser_context) {
#if defined(OS_ANDROID)
  NOTIMPLEMENTED();
#else
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DCHECK(profile);

  if (switches::SettingsWindowEnabled()) {
    chrome::ShowContentSettingsExceptionsInWindow(
        profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  } else {
    chrome::ScopedTabbedBrowserDisplayer browser_displayer(profile);
    chrome::ShowContentSettingsExceptions(browser_displayer.browser(),
                                          CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  }
#endif  // defined(OS_ANDROID)
}
