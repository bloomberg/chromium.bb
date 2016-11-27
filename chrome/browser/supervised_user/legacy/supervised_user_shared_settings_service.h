// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_LEGACY_SUPERVISED_USER_SHARED_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_SUPERVISED_USER_LEGACY_SUPERVISED_USER_SHARED_SETTINGS_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/callback_list.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/syncable_service.h"

class PrefService;

namespace base {
class Value;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// SupervisedUserSharedSettingsService syncs settings (as key-value pairs) that
// can be modified both by a supervised user and their manager.
// A supervised user can only modify their own settings, whereas a manager can
// modify settings for all their supervised users.
//
// The shared settings are stored in the user preferences in a multi-level
// dictionary. The first level is the SU ID (called "managed user ID" on the
// server for historic reasons), the second level is the key for the setting,
// and the third level is a dictionary with a "value" key for the value and an
// "acknowledged" flag, which is used to wait for the Sync server to acknowledge
// that it has seen a setting change (see SupervisedUserSharedSettingsUpdate for
// how to use this).
class SupervisedUserSharedSettingsService : public KeyedService,
                                            public syncer::SyncableService {
 public:
  // Called whenever a setting changes (see Subscribe() below).
  typedef base::Callback<void(const std::string& /* su_id */,
                              const std::string& /* key */)> ChangeCallback;
  typedef base::CallbackList<
      void(const std::string& /* su_id */, const std::string& /* key */)>
      ChangeCallbackList;

  // This constructor is public only for testing. Use
  // |SupervisedUserSharedSettingsServiceFactory::GetForProfile(...)| instead to
  // get an instance of this service in production code.
  explicit SupervisedUserSharedSettingsService(PrefService* prefs);
  ~SupervisedUserSharedSettingsService() override;

  // Returns the value for the given |key| and the supervised user identified by
  // |su_id|. If either the supervised user or the key does not exist, NULL is
  // returned. Note that if the profile that owns this service belongs to a
  // supervised user, callers will only see settings for their own |su_id|, i.e.
  // a non-matching |su_id| is treated as non-existent.
  const base::Value* GetValue(const std::string& su_id, const std::string& key);

  // Sets the value for the given |key| and the supervised user identified by
  // |su_id|. If the profile that owns this service belongs to a supervised
  // user, |su_id| must be their own.
  void SetValue(const std::string& su_id,
                const std::string& key,
                const base::Value& value);

  // Subscribes to changes in the synced settings. The callback will be notified
  // whenever any setting for any supervised user is changed via Sync (but not
  // for local changes). Subscribers should filter the settings and users they
  // are interested in with the |su_id| and |key| parameters to the callback.
  std::unique_ptr<ChangeCallbackList::Subscription> Subscribe(
      const ChangeCallback& cb);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Public for testing.
  void SetValueInternal(const std::string& su_id,
                        const std::string& key,
                        const base::Value& value,
                        bool acknowledged);

  // Public for testing.
  static syncer::SyncData CreateSyncDataForSetting(const std::string& su_id,
                                                   const std::string& key,
                                                   const base::Value& value,
                                                   bool acknowledged);

  // KeyedService implementation:
  void Shutdown() override;

  // SyncableService implementation:
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

 private:
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;
  std::unique_ptr<syncer::SyncErrorFactory> error_handler_;

  ChangeCallbackList callbacks_;

  PrefService* prefs_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_LEGACY_SUPERVISED_USER_SHARED_SETTINGS_SERVICE_H_
