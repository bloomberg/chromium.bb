// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/chrome_sync_notification_bridge.h"

#include "chrome/browser/sync/notifier/sync_notifier_observer.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;

namespace sync_notifier {

ChromeSyncNotificationBridge::ChromeSyncNotificationBridge(
    const Profile* profile)
    : observers_(new ObserverListThreadSafe<SyncNotifierObserver>()) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile);
  registrar_.Add(this, chrome::NOTIFICATION_SYNC_REFRESH,
                 content::Source<Profile>(profile));
}

ChromeSyncNotificationBridge::~ChromeSyncNotificationBridge() {}

void ChromeSyncNotificationBridge::AddObserver(SyncNotifierObserver* observer) {
  observers_->AddObserver(observer);
}

void ChromeSyncNotificationBridge::RemoveObserver(
    SyncNotifierObserver* observer) {
  observers_->RemoveObserver(observer);
}

void ChromeSyncNotificationBridge::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(type, chrome::NOTIFICATION_SYNC_REFRESH);
  content::Details<const syncable::ModelType> model_type_details(details);
  const syncable::ModelType model_type = *(model_type_details.ptr());

  // Currently, we only expect SESSIONS to trigger this notification.
  DCHECK_EQ(syncable::SESSIONS, model_type);
  syncable::ModelTypePayloadMap payload_map;
  payload_map[model_type] = "";
  observers_->Notify(&SyncNotifierObserver::OnIncomingNotification,
                     payload_map, LOCAL_NOTIFICATION);
}

}  // namespace sync_notifier
