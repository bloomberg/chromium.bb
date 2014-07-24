// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The ChromeNotifierService works together with sync to maintain the state of
// user notifications, which can then be presented in the notification center,
// via the Notification UI Manager.

#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/synced_notifications_private/synced_notifications_shim.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/event_router.h"

namespace notifier {

ChromeNotifierService::ChromeNotifierService(Profile* profile)
    : profile_(profile),
      weak_ptr_factory_(this) {
  synced_notifications_shim_.reset(new SyncedNotificationsShim(
      base::Bind(&ChromeNotifierService::FireSyncJSEvent,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&ChromeNotifierService::NotifyRefreshNeeded,
                 weak_ptr_factory_.GetWeakPtr())));
}

ChromeNotifierService::~ChromeNotifierService() {
}

// Methods from KeyedService.
void ChromeNotifierService::Shutdown() {}

SyncedNotificationsShim* ChromeNotifierService::GetSyncedNotificationsShim() {
  return synced_notifications_shim_.get();
}

void ChromeNotifierService::FireSyncJSEvent(
    scoped_ptr<extensions::Event> event) {
  event->restrict_to_browser_context = profile_;
  // TODO(synced notifications): consider broadcasting to a specific extension
  // id.
  extensions::EventRouter::Get(profile_)->BroadcastEvent(event.Pass());
}

void ChromeNotifierService::NotifyRefreshNeeded() {
  const syncer::ModelTypeSet types(syncer::SYNCED_NOTIFICATIONS);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
      content::Source<Profile>(profile_),
      content::Details<const syncer::ModelTypeSet>(&types));
}

}  // namespace notifier
