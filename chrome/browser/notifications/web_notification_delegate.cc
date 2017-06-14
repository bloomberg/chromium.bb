// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/web_notification_delegate.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/nullable_string16.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
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
    NotificationCommon::Type notification_type,
    Profile* profile,
    const std::string& notification_id,
    const GURL& origin)
    : notification_type_(notification_type),
      profile_(profile),
      notification_id_(notification_id),
      origin_(origin) {}

WebNotificationDelegate::~WebNotificationDelegate() {}

std::string WebNotificationDelegate::id() const {
  return notification_id_;
}

bool WebNotificationDelegate::SettingsClick() {
#if !defined(OS_CHROMEOS)
  NotificationCommon::OpenNotificationSettings(profile_);
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
  // Check to see if this notification comes from a webpage that is displaying
  // fullscreen content.
  for (auto* browser : *BrowserList::GetInstance()) {
    // Only consider the browsers for the profile that created the notification
    if (browser->profile() != profile_)
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

void WebNotificationDelegate::Display() {}

void WebNotificationDelegate::Close(bool by_user) {
  auto* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  display_service->ProcessNotificationOperation(
      NotificationCommon::CLOSE, notification_type_, origin().spec(),
      notification_id_, -1, base::NullableString16());
}

void WebNotificationDelegate::Click() {
  auto* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  display_service->ProcessNotificationOperation(
      NotificationCommon::CLICK, notification_type_, origin().spec(),
      notification_id_, -1, base::NullableString16());
}

void WebNotificationDelegate::ButtonClick(int button_index) {
  DCHECK(button_index >= 0);
  auto* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  display_service->ProcessNotificationOperation(
      NotificationCommon::CLICK, notification_type_, origin().spec(),
      notification_id_, button_index, base::NullableString16());
}

void WebNotificationDelegate::ButtonClickWithReply(
    int button_index,
    const base::string16& reply) {
  auto* display_service =
      NotificationDisplayServiceFactory::GetForProfile(profile_);
  display_service->ProcessNotificationOperation(
      NotificationCommon::CLICK, notification_type_, origin().spec(),
      notification_id_, button_index,
      base::NullableString16(reply, false /* is_null */));
}
