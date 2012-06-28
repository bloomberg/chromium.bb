// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/chrome_sync_notification_bridge.h"

#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "sync/notifier/sync_notifier_observer.h"

using content::BrowserThread;

namespace browser_sync {

ChromeSyncNotificationBridge::ChromeSyncNotificationBridge(
    const Profile* profile)
    : observers_(
        new ObserverListThreadSafe<syncer::SyncNotifierObserver>()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);
  registrar_.Add(this, chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
                 content::Source<Profile>(profile));
  registrar_.Add(this, chrome::NOTIFICATION_SYNC_REFRESH_REMOTE,
                 content::Source<Profile>(profile));
}

ChromeSyncNotificationBridge::~ChromeSyncNotificationBridge() {}

void ChromeSyncNotificationBridge::UpdateEnabledTypes(
    const syncable::ModelTypeSet types) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  enabled_types_ = types;
}

void ChromeSyncNotificationBridge::AddObserver(
    syncer::SyncNotifierObserver* observer) {
  observers_->AddObserver(observer);
}

void ChromeSyncNotificationBridge::RemoveObserver(
    syncer::SyncNotifierObserver* observer) {
  observers_->RemoveObserver(observer);
}

void ChromeSyncNotificationBridge::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  syncer::IncomingNotificationSource notification_source;
  if (type == chrome::NOTIFICATION_SYNC_REFRESH_LOCAL) {
    notification_source = syncer::LOCAL_NOTIFICATION;
  } else if (type == chrome::NOTIFICATION_SYNC_REFRESH_REMOTE) {
    notification_source = syncer::REMOTE_NOTIFICATION;
  } else {
    NOTREACHED() << "Unexpected notification type:" << type;
    return;
  }

  content::Details<const syncable::ModelTypePayloadMap>
      payload_details(details);
  syncable::ModelTypePayloadMap payload_map = *(payload_details.ptr());

  if (payload_map.empty()) {
    // No model types to invalidate, invalidating all enabled types.
    payload_map =
        syncable::ModelTypePayloadMapFromEnumSet(enabled_types_, std::string());
  }

  observers_->Notify(
      &syncer::SyncNotifierObserver::OnIncomingNotification,
      payload_map, notification_source);
}

}  // namespace browser_sync
