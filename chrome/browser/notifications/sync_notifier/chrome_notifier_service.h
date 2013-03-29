// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "sync/api/syncable_service.h"

class NotificationUIManager;
class Profile;

namespace notifier {

// The ChromeNotifierService holds notifications which represent the state of
// delivered notifications for chrome. These are obtained from the sync service
// and kept up to date.
class ChromeNotifierService : public syncer::SyncableService,
                              public ProfileKeyedService {
 public:
  ChromeNotifierService(Profile* profile, NotificationUIManager* manager);
  virtual ~ChromeNotifierService();

  // Methods from ProfileKeyedService.
  virtual void Shutdown() OVERRIDE;

  // syncer::SyncableService implementation.
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(
      syncer::ModelType type) const OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  // Convert from internal representation to SyncData representation.
  static syncer::SyncData CreateSyncDataFromNotification(
      const SyncedNotification& notification);

  // Convert from SyncData representation to internal representation.
  static scoped_ptr<SyncedNotification> CreateNotificationFromSyncData(
      const syncer::SyncData& sync_data);

  // Get a pointer to a notification.  ChromeNotifierService owns this pointer.
  // The caller must not free it.
  notifier::SyncedNotification* FindNotificationByKey(const std::string& key);

  // Called when we dismiss a notification.
  void MarkNotificationAsDismissed(const std::string& id);

  // functions for test
  void AddForTest(scoped_ptr<notifier::SyncedNotification> notification) {
    Add(notification.Pass());
  }

 private:
  // Add a notification to our list.  This takes ownership of the pointer.
  void Add(scoped_ptr<notifier::SyncedNotification> notification);

  // Display a notification in the notification center.
  void Show(notifier::SyncedNotification* notification);

  // Back pointer to the owning profile.
  Profile* const profile_;
  NotificationUIManager* const notification_manager_;
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // TODO(petewil): consider whether a map would better suit our data.
  // If there are many entries, lookup time may trump locality of reference.
  ScopedVector<notifier::SyncedNotification> notification_data_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNotifierService);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_H_
