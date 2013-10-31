// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/sync_notifier/chrome_notifier_delegate.h"


#include "base/metrics/histogram.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/user_metrics.h"

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

void ChromeNotifierDelegate::CollectAction(SyncedNotificationActionType type) {
  DCHECK(!notification_id_.empty());

  UMA_HISTOGRAM_ENUMERATION("SyncedNotifications.Actions",
                            type,
                            SYNCED_NOTIFICATION_ACTION_COUNT);
}


// TODO(petewil) Add the ability to do URL actions also.
void ChromeNotifierDelegate::Click() {
  SyncedNotification* notification =
      chrome_notifier_->FindNotificationById(notification_id_);
  if (notification == NULL)
    return;

  GURL destination = notification->GetDefaultDestinationUrl();
  NavigateToUrl(destination);
  // TODO(petewil): Once the service protobuf supports a viewed state, mark the
  // notification as viewed here.

  // Record the action in UMA statistics.
  CollectAction(SYNCED_NOTIFICATION_ACTION_CLICK);
}

// TODO(petewil) Add the ability to do URL actions also.
void ChromeNotifierDelegate::ButtonClick(int button_index) {
  SyncedNotification* notification =
      chrome_notifier_->FindNotificationById(notification_id_);
  if (notification) {
    GURL destination = notification->GetButtonUrl(button_index);
    NavigateToUrl(destination);
    // TODO(petewil): Once the service protobuf supports a viewed state, mark
    // the notification as viewed here.
  }

  // Now record the UMA statistics for this action.
  CollectAction(SYNCED_NOTIFICATION_ACTION_BUTTON_CLICK);
}

void ChromeNotifierDelegate::NavigateToUrl(const GURL& destination) const {
  if (!destination.is_valid())
    return;

  // Navigate to the URL in a new tab.
  content::OpenURLParams open_params(destination, content::Referrer(),
                                    NEW_FOREGROUND_TAB,
                                    content::PAGE_TRANSITION_LINK, false);
  chrome::ScopedTabbedBrowserDisplayer displayer(
      chrome_notifier_->profile(), chrome::GetActiveDesktop());
  displayer.browser()->OpenURL(open_params);
  displayer.browser()->window()->Activate();
}

void ChromeNotifierDelegate::Close(bool by_user) {
  if (by_user)
    chrome_notifier_->MarkNotificationAsRead(notification_id_);

  CollectAction(by_user ?
      SYNCED_NOTIFICATION_ACTION_CLOSE_BY_USER :
      SYNCED_NOTIFICATION_ACTION_CLOSE_BY_SYSTEM);
}

}  // namespace notifier
