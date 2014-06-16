// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/sync_notifier/synced_notification.h"
#include "components/keyed_service/core/keyed_service.h"
#include "sync/api/syncable_service.h"
#include "ui/message_center/notifier_settings.h"

class NotificationUIManager;
class Profile;
class SyncedNotificationsShim;

namespace extensions {
struct Event;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace message_center {
struct Notifier;
}

namespace notifier {
class SyncedNotificationAppInfo;
class SyncedNotificationAppInfoService;

// The name of our first synced notification service.
// TODO(petewil): Remove this once we figure out how to do UMA for each sending
// service name without knowing the name in advance.
extern const char kFirstSyncedNotificationServiceId[];
extern const char kServiceEnabledOnce[];
extern const char kSyncedNotificationFirstRun[];
extern const char kSyncedNotificationsWelcomeOrigin[];

enum ChromeNotifierServiceActionType {
  CHROME_NOTIFIER_SERVICE_ACTION_UNKNOWN,
  CHROME_NOTIFIER_SERVICE_ACTION_FIRST_SERVICE_ENABLED,
  CHROME_NOTIFIER_SERVICE_ACTION_FIRST_SERVICE_DISABLED,
  // NOTE: Add new action types only immediately above this line. Also,
  // make sure the enum list in tools/histogram/histograms.xml is
  // updated with any change in here.
  CHROME_NOTIFIER_SERVICE_ACTION_COUNT
};

// The ChromeNotifierService holds notifications which represent the state of
// delivered notifications for chrome. These are obtained from the sync service
// and kept up to date.
class ChromeNotifierService : public syncer::SyncableService,
                              public KeyedService {
 public:
  ChromeNotifierService(Profile* profile, NotificationUIManager* manager);
  virtual ~ChromeNotifierService();

  // Methods from KeyedService.
  virtual void Shutdown() OVERRIDE;

  // Returns the SyncableService for syncer::SYNCED_NOTIFICATIONS and
  // syncer::SYNCED_NOTIFICATION_APP_INFO
  SyncedNotificationsShim* GetSyncedNotificationsShim();

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
  scoped_ptr<SyncedNotification> CreateNotificationFromSyncData(
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

  // When app ids are added or removed, unblock or remove associated messages.
  void OnAddedAppIds(std::vector<std::string> added_app_ids);
  void OnRemovedAppIds(std::vector<std::string> removed_app_ids);

  // Register the preferences we use to save state.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  Profile* profile() const { return profile_; }

  // Find and retun the sending service Id for this notification.
  std::string GetSendingServiceId(
      const SyncedNotification* synced_notification);

  // Functions for test.
  void AddForTest(scoped_ptr<notifier::SyncedNotification> notification);

  void SetSyncedNotificationAppInfoServiceForTest(
      SyncedNotificationAppInfoService* synced_notification_app_info_service);

  // If we allow the tests to do bitmap fetching, they will attempt to fetch
  // a URL from the web, which will fail.  We can already test the majority
  // of what we want without also trying to fetch bitmaps.  Other tests will
  // cover bitmap fetching.
  static void set_avoid_bitmap_fetching_for_test(bool avoid) {
    avoid_bitmap_fetching_for_test_ = avoid;
  }

  // Initialize the preferences we use for the ChromeNotificationService.
  void InitializePrefs();

  void ShowWelcomeToastIfNecessary(
      const SyncedNotification* notification,
      NotificationUIManager* notification_ui_manager);

 private:
  // Helper method for firing JS events triggered by sync.
  void FireSyncJSEvent(scoped_ptr<extensions::Event> event);

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

  // When we turn a sending service on or off, collect statistics about
  // how often users turn it on or off.
  void CollectPerServiceEnablingStatistics(const std::string& notifier_id,
                                           bool enabled);

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

  // Finds an app info by using the AppId
  notifier::SyncedNotificationAppInfo* FindAppInfoByAppId(
      const std::string& app_id) const;

  // Builds a welcome notification for the listed sending service.
  const Notification CreateWelcomeNotificationForService(
      SyncedNotificationAppInfo* app_info);

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
  base::ThreadChecker thread_checker_;
  // Unowned pointer to the App Info service.  The lifetime is managed by the
  // profile service, this service depends on the App Info service, so it should
  // always be in scope whenever our service is active.
  SyncedNotificationAppInfoService* synced_notification_app_info_service_;

  // TODO(petewil): Consider whether a map would better suit our data.
  // If there are many entries, lookup time may trump locality of reference.
  ScopedVector<SyncedNotification> notification_data_;

  // Shim connecting the JS private api to sync. // TODO(zea): delete all other
  // code.
  scoped_ptr<SyncedNotificationsShim> synced_notifications_shim_;

  base::WeakPtrFactory<ChromeNotifierService> weak_ptr_factory_;

  friend class ChromeNotifierServiceTest;
  FRIEND_TEST_ALL_PREFIXES(ChromeNotifierServiceTest, ServiceEnabledTest);
  FRIEND_TEST_ALL_PREFIXES(ChromeNotifierServiceTest,
                           AddNewSendingServicesTest);
  FRIEND_TEST_ALL_PREFIXES(ChromeNotifierServiceTest,
                           CheckInitializedServicesTest);
  FRIEND_TEST_ALL_PREFIXES(ChromeNotifierServiceTest,
                           GetEnabledSendingServicesFromPreferencesTest);
  FRIEND_TEST_ALL_PREFIXES(ChromeNotifierServiceTest, CheckFindAppInfo);
  FRIEND_TEST_ALL_PREFIXES(ChromeNotifierServiceTest, SetAddedAppIdsTest);
  FRIEND_TEST_ALL_PREFIXES(ChromeNotifierServiceTest, SetRemovedAppIdsTest);

  DISALLOW_COPY_AND_ASSIGN(ChromeNotifierService);
};

}  // namespace notifier

#endif  // CHROME_BROWSER_NOTIFICATIONS_SYNC_NOTIFIER_CHROME_NOTIFIER_SERVICE_H_
