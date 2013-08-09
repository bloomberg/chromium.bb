// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/chrome_notifier_delegate.h"

#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/browser/page_navigator.h"

namespace notifier {
ChromeNotifierDelegate::ChromeNotifierDelegate(
    const std::string& notification_id,
    ChromeNotifierService* notifier)
    : notification_id_(notification_id), chrome_notifier_(notifier) {}

ChromeNotifierDelegate::~ChromeNotifierDelegate() {}

std::string ChromeNotifierDelegate::id() const {
   return notification_id_;
}

content::RenderViewHost* ChromeNotifierDelegate::GetRenderViewHost() const {
    return NULL;
}

// TODO(petewil) Add the ability to do URL actions also.
void ChromeNotifierDelegate::Click() {
  SyncedNotification* notification =
      chrome_notifier_->FindNotificationById(notification_id_);
  if (notification == NULL)
    return;

  GURL destination = notification->GetDefaultDestinationUrl();
  NavigateToUrl(destination);
}

// TODO(petewil) Add the ability to do URL actions also.
void ChromeNotifierDelegate::ButtonClick(int button_index) {
  SyncedNotification* notification =
      chrome_notifier_->FindNotificationById(notification_id_);
  if (notification) {
    GURL destination = notification->GetButtonUrl(button_index);
    NavigateToUrl(destination);
  }
}

void ChromeNotifierDelegate::NavigateToUrl(const GURL& destination) const {
  if (!destination.is_valid())
    return;

  content::OpenURLParams openParams(destination, content::Referrer(),
                                    NEW_FOREGROUND_TAB,
                                    content::PAGE_TRANSITION_LINK, false);
  Browser* browser = chrome::FindLastActiveWithProfile(
      chrome_notifier_->profile(),
      chrome::GetActiveDesktop());
  // Navigate to the URL in a new tab.
  if (browser != NULL)
    browser->OpenURL(openParams);

}

void ChromeNotifierDelegate::Close(bool by_user) {
  if (by_user)
    chrome_notifier_->MarkNotificationAsRead(notification_id_);
}

}  // namespace notifier
