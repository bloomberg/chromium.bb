// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_MANAGER_H_
#pragma once

#include <map>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/extensions/app_notification.h"
#include "chrome/browser/extensions/app_notification_storage.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "sync/api/sync_change.h"
#include "sync/api/syncable_service.h"

class PerfTimer;
class Profile;
class SyncErrorFactory;

// This class keeps track of notifications for installed apps.
class AppNotificationManager
    : public base::RefCountedThreadSafe<
          AppNotificationManager,
          content::BrowserThread::DeleteOnUIThread>,
      public content::NotificationObserver,
      public SyncableService {
 public:
  static const unsigned int kMaxNotificationPerApp;
  explicit AppNotificationManager(Profile* profile);

  // Starts up the process of reading from persistent storage.
  void Init();

  // Returns whether add was succcessful.
  // Takes ownership of |item| in all cases.
  bool Add(AppNotification* item);

  const AppNotificationList* GetAll(const std::string& extension_id) const;

  // Returns the most recently added notification for |extension_id| if there
  // are any, or NULL otherwise.
  const AppNotification* GetLast(const std::string& extension_id);

  // Clears all notifications for |extension_id|.
  void ClearAll(const std::string& extension_id);

  // Implementing content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  bool loaded() const { return notifications_.get() != NULL; }

  // SyncableService implementation.

  // Returns all syncable notifications from this model as SyncData.
  virtual SyncDataList GetAllSyncData(syncable::ModelType type) const OVERRIDE;
  // Process notifications related changes from Sync, merging them into
  // our model.
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE;
  // Associate and merge sync data model with our data model.
  virtual SyncError MergeDataAndStartSyncing(
      syncable::ModelType type,
      const SyncDataList& initial_sync_data,
      scoped_ptr<SyncChangeProcessor> sync_processor,
      scoped_ptr<SyncErrorFactory> sync_error_factory) OVERRIDE;
  virtual void StopSyncing(syncable::ModelType type) OVERRIDE;

 private:
  friend class AppNotificationManagerSyncTest;
  friend class base::DeleteHelper<AppNotificationManager>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  FRIEND_TEST_ALL_PREFIXES(AppNotificationManagerSyncTest,
                           NotificationToSyncDataToNotification);
  FRIEND_TEST_ALL_PREFIXES(AppNotificationManagerSyncTest,
                           GetAllSyncDataNoLocal);
  FRIEND_TEST_ALL_PREFIXES(AppNotificationManagerSyncTest,
                           GetAllSyncDataSomeLocal);
  FRIEND_TEST_ALL_PREFIXES(AppNotificationManagerSyncTest,
                           ModelAssocModelEmpty);
  FRIEND_TEST_ALL_PREFIXES(AppNotificationManagerSyncTest,
                           ModelAssocBothNonEmptyNoOverlap);
  FRIEND_TEST_ALL_PREFIXES(AppNotificationManagerSyncTest,
                           ModelAssocBothNonEmptySomeOverlap);
  FRIEND_TEST_ALL_PREFIXES(AppNotificationManagerSyncTest,
                           ModelAssocBothNonEmptyTitleMismatch);
  FRIEND_TEST_ALL_PREFIXES(AppNotificationManagerSyncTest,
                           ModelAssocBothNonEmptyMatchesLocal);
  FRIEND_TEST_ALL_PREFIXES(AppNotificationManagerSyncTest,
                           ProcessSyncChangesErrorModelAssocNotDone);
  FRIEND_TEST_ALL_PREFIXES(AppNotificationManagerSyncTest,
                           ProcessSyncChangesEmptyModel);
  FRIEND_TEST_ALL_PREFIXES(AppNotificationManagerSyncTest,
                           StopSyncing);
  FRIEND_TEST_ALL_PREFIXES(AppNotificationManagerSyncTest,
                           ClearAllGetsSynced);

  // Maps extension id to a list of notifications for that extension.
  typedef std::map<std::string, AppNotificationList> NotificationMap;

  virtual ~AppNotificationManager();

  // Starts loading storage_ using |storage_path|.
  void LoadOnFileThread(const FilePath& storage_path);

  // Called on the UI thread to handle the loaded results from storage_.
  void HandleLoadResults(NotificationMap* map);

  // Saves the contents of |list| to storage.
  // Ownership of |list| is transferred here.
  void SaveOnFileThread(const std::string& extension_id,
                        AppNotificationList* list);

  void DeleteOnFileThread(const std::string& extension_id);

  // Gets notficiations for a given extension id.
  AppNotificationList& GetAllInternal(const std::string& extension_id);

  // Removes the notification with given extension id and given guid.
  void Remove(const std::string& extension_id, const std::string& guid);

  // Returns the notification for the given |extension_id| and |guid|,
  // NULL if no such notification exists.
  const AppNotification* GetNotification(const std::string& extension_id,
                                         const std::string& guid);

  // Sends a change to syncer to add the given notification.
  void SyncAddChange(const AppNotification& notif);

  // Sends a change to syncer to remove the given notification.
  void SyncRemoveChange(const AppNotification& notif);

  // Sends changes to syncer to remove all notifications in the given list.
  void SyncClearAllChange(const AppNotificationList& list);

  // Converters from AppNotification to SyncData and vice versa.
  static SyncData CreateSyncDataFromNotification(
      const AppNotification& notification);
  static AppNotification* CreateNotificationFromSyncData(
      const SyncData& sync_data);

  Profile* profile_;
  content::NotificationRegistrar registrar_;
  scoped_ptr<NotificationMap> notifications_;

  // This should only be used on the FILE thread.
  scoped_ptr<AppNotificationStorage> storage_;

  // Sync change processor we use to push all our changes.
  scoped_ptr<SyncChangeProcessor> sync_processor_;

  // Sync error handler that we use to create errors from.
  scoped_ptr<SyncErrorFactory> sync_error_factory_;

  // Whether the sync model is associated with the local model.
  // In other words, whether we are ready to apply sync changes.
  bool models_associated_;
  // Whether syncer changes are being processed right now.
  bool processing_syncer_changes_;

  // Used for a histogram of load time.
  scoped_ptr<PerfTimer> load_timer_;

  DISALLOW_COPY_AND_ASSIGN(AppNotificationManager);
};

#endif  // CHROME_BROWSER_EXTENSIONS_APP_NOTIFICATION_MANAGER_H_
