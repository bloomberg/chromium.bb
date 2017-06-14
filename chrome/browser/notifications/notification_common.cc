// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_common.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_context.h"
#include "ui/message_center/notifier_settings.h"

namespace features {

const base::Feature kAllowFullscreenWebNotificationsFeature{
    "FSNotificationsWeb", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features

// static
void NotificationCommon::OpenNotificationSettings(
    content::BrowserContext* browser_context) {
#if defined(OS_ANDROID)
  // Android settings are opened directly from Java
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

// static
bool NotificationCommon::ShouldDisplayOnFullScreen(Profile* profile,
                                                   const GURL& origin) {
#if defined(OS_ANDROID)
  NOTIMPLEMENTED();
  return false;
#endif  // defined(OS_ANDROID)

  // Check to see if this notification comes from a webpage that is displaying
  // fullscreen content.
  for (auto* browser : *BrowserList::GetInstance()) {
    // Only consider the browsers for the profile that created the notification
    if (browser->profile() != profile)
      continue;

    const content::WebContents* active_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    if (!active_contents)
      continue;

    // Check to see if
    //  (a) the active tab in the browser shares its origin with the
    //      notification.
    //  (b) the browser is fullscreen
    //  (c) the browser has focus.
    if (active_contents->GetURL().GetOrigin() == origin &&
        browser->exclusive_access_manager()->context()->IsFullscreen() &&
        browser->window()->IsActive()) {
      bool enabled = base::FeatureList::IsEnabled(
          features::kAllowFullscreenWebNotificationsFeature);
      if (enabled) {
        UMA_HISTOGRAM_ENUMERATION("Notifications.Display_Fullscreen.Shown",
                                  message_center::NotifierId::WEB_PAGE,
                                  message_center::NotifierId::SIZE);
      } else {
        UMA_HISTOGRAM_ENUMERATION("Notifications.Display_Fullscreen.Suppressed",
                                  message_center::NotifierId::WEB_PAGE,
                                  message_center::NotifierId::SIZE);
      }
      return enabled;
    }
  }
  return false;
}
