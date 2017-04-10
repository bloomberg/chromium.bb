// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/web_notification_delegate.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "ui/message_center/notifier_settings.h"

using message_center::NotifierId;

namespace features {

const base::Feature kAllowFullscreenWebNotificationsFeature{
  "FSNotificationsWeb", base::FEATURE_ENABLED_BY_DEFAULT
};

} // namespace features


WebNotificationDelegate::WebNotificationDelegate(
    content::BrowserContext* browser_context,
    const std::string& notification_id,
    const GURL& origin)
    : browser_context_(browser_context),
      notification_id_(notification_id),
      origin_(origin) {}

WebNotificationDelegate::~WebNotificationDelegate() {}

std::string WebNotificationDelegate::id() const {
  return notification_id_;
}

bool WebNotificationDelegate::SettingsClick() {
#if !defined(OS_CHROMEOS)
  NotificationCommon::OpenNotificationSettings(browser_context_);
  return true;
#else
  return false;
#endif
}

bool WebNotificationDelegate::ShouldDisplaySettingsButton() {
  return true;
}

bool WebNotificationDelegate::ShouldDisplayOverFullscreen() const {
#if !defined(OS_ANDROID)
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  DCHECK(profile);
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
    if (active_contents->GetURL().GetOrigin() == origin_ &&
        browser->exclusive_access_manager()->context()->IsFullscreen() &&
        browser->window()->IsActive()) {
      bool enabled = base::FeatureList::IsEnabled(
          features::kAllowFullscreenWebNotificationsFeature);
      if (enabled) {
        UMA_HISTOGRAM_ENUMERATION("Notifications.Display_Fullscreen.Shown",
                                  NotifierId::WEB_PAGE,
                                  NotifierId::SIZE);
      } else {
        UMA_HISTOGRAM_ENUMERATION(
            "Notifications.Display_Fullscreen.Suppressed",
            NotifierId::WEB_PAGE,
            NotifierId::SIZE);
      }
      return enabled;
    }
  }
#endif

  return false;
}
