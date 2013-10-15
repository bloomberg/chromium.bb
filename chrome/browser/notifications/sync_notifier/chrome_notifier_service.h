// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "base/prefs/pref_member.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "sync/api/syncable_service.h"

class NotificationUIManager;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace message_center {
struct Notifier;
}

namespace notifier {

extern const char kFirstSyncedNotificationServiceId[];
extern const char kServiceEnabledOnce[];
extern const char kSyncedNotificationFirstRun[];

// The ChromeNotifierService holds notifications which represent the state of
// delivered notifications for chrome. These are obtained from the sync service
// and kept up to date.
class ChromeNotifierService : public syncer::SyncableService,
                              public BrowserContextKeyedService {
 public:
  ChromeNotifierService(Profile* profile, NotificationUIManager* manager);
  virtual ~ChromeNotifierService();

  // Methods from BrowserContextKeyedService.
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
  virtual notifier::SyncedNotification* FindNotificationById(
      const std::string& notification_id);

  // Get the list of synced notification services and fill their meta data to
  // |notifiers|.
  void GetSyncedNotificationServices(
      std::vector<message_center::Notifier*>* notifiers);

  // Called when we dismiss a notification.  This is virtual so that test
  // subclasses can override it.
  virtual void MarkNotificationAsRead(const std::string& id);

  // Called when a notier is enabled or disabled.
  void OnSyncedNotificationServiceEnabled(
      const std::string& notifier_id,
      bool enabled);

  // Register the preferences we use to save state.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  Profile* profile() const { return profile_; }

  // Functions for test.
  void AddForTest(scoped_ptr<notifier::SyncedNotification> notification);

  // If we allow the tests to do bitmap fetching, they will attempt to fetch
  // a URL from the web, which will fail.  We can already test the majority
  // of what we want without also trying to fetch bitmaps.  Other tests will
  // cover bitmap fetching.
  static void set_avoid_bitmap_fetching_for_test(bool avoid) {
    avoid_bitmap_fetching_for_test_ = avoid;
  }

  // Initialize the preferences we use for the ChromeNotificationService.
  void InitializePrefs();

 private:
  // Add a notification to our list.  This takes ownership of the pointer.
  void Add(scoped_ptr<notifier::SyncedNotification> notification);

  // Display this notification in the notification center, or remove it.
  void UpdateInMessageCenter(notifier::SyncedNotification* notification);

  // Display a notification in the notification center (eventually).
  void Display(notifier::SyncedNotification* notification);

  // Remove a notification from our store.
  void FreeNotificationById(const std::string& notification_id);

  // When a service it turned on, scan our cache for any notifications
  // for that service, and display them if they are unread.
  void DisplayUnreadNotificationsFromSource(const std::string& notifier_id);

  // When a service it turned off, scan our cache for any notifications
  // for that service, and remove them from the message center.
  void RemoveUnreadNotificationsFromSource(const std::string& notifier_id);

  // When we start up or hear of a new service, turn it on by default.
  void AddNewSendingServices();

  // Called when the string list pref has been changed.
  void OnEnabledSendingServiceListPrefChanged(std::set<std::string>* ids_field);

  // Called when the string list pref has been changed.
  void OnInitializedSendingServiceListPrefChanged(
      std::set<std::string>* ids_field);

  // Called when our "first run" boolean pref has been changed.
  void OnSyncedNotificationFirstRunBooleanPrefChanged(bool* new_value);

  // Convert our internal set of strings to a list value.
  // The second param is an outparam which the function fills in.
  void BuildServiceListValueInplace(
      std::set<std::string> services, base::ListValue* list_value);

  // Preferences for storing which SyncedNotificationServices are enabled
  StringListPrefMember enabled_sending_services_prefs_;
  StringListPrefMember initialized_sending_services_prefs_;

  // Preferences to avoid toasting on SyncedNotification first run.
  BooleanPrefMember synced_notification_first_run_prefs_;

  // Back pointer to the owning profile.
  Profile* const profile_;
  NotificationUIManager* const notification_manager_;
  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;
  std::set<std::string> enabled_sending_services_;
  std::set<std::string> initialized_sending_services_;
  bool synced_notification_first_run_;
  static bool avoid_bitmap_fetching_for_test_;

  // TODO(petewil): Consider whether a map would better suit our data.
  // If there are many entries, lookup time may trump locality of reference.
  ScopedVector<notifier::SyncedNotification> notification_data_;

  friend class ChromeNotifierServiceTest;
  FRIEND_TEST_ALL_PREFIXES(ChromeNotifierServiceTest, ServiceEnabledTest);
  FRIEND_TEST_ALL_PREFIXES(ChromeNotifierServiceTest,
                           AddNewSendingServicesTest);
  FRIEND_TEST_ALL_PREFIXES(ChromeNotifierServiceTest,
                           GetEnabledSendingServicesFromPreferencesTest);


  DISALLOW_COPY_AND_ASSIGN(ChromeNotifierService);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_H_
