// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_CHROME_SYNC_NOTIFICATION_BRIDGE_H_
#define CHROME_BROWSER_SYNC_GLUE_CHROME_SYNC_NOTIFICATION_BRIDGE_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "sync/internal_api/public/syncable/model_type.h"

class Profile;

namespace sync_notifier {
class SyncNotifierObserver;
}  // namespace

namespace browser_sync {

// A thread-safe bridge for chrome events that can trigger sync notifications.
// Listens to NOTIFICATION_SYNC_REFRESH_LOCAL and
// NOTIFICATION_SYNC_REFRESH_REMOTE and triggers each observer's
// OnIncomingNotification method on these notifications.
// Note: Notifications are expected to arrive on the UI thread, but observers
// may live on any thread.
class ChromeSyncNotificationBridge : public content::NotificationObserver {
 public:
  explicit ChromeSyncNotificationBridge(const Profile* profile);
  virtual ~ChromeSyncNotificationBridge();

  // Must be called on UI thread.
  void UpdateEnabledTypes(const syncable::ModelTypeSet enabled_types);

  // These can be called on any thread.
  virtual void AddObserver(sync_notifier::SyncNotifierObserver* observer);
  virtual void RemoveObserver(sync_notifier::SyncNotifierObserver* observer);

  // NotificationObserver implementation. Called on UI thread.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  content::NotificationRegistrar registrar_;
  syncable::ModelTypeSet enabled_types_;

  // Because [Add/Remove]Observer can be called from any thread, we need a
  // thread-safe observerlist.
  scoped_refptr<ObserverListThreadSafe<sync_notifier::SyncNotifierObserver> >
      observers_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_CHROME_SYNC_NOTIFICATION_BRIDGE_H_
