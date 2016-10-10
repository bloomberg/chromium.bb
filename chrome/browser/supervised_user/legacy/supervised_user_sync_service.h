// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_LEGACY_SUPERVISED_USER_SYNC_SERVICE_H_
#define CHROME_BROWSER_SUPERVISED_USER_LEGACY_SUPERVISED_USER_SYNC_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service_observer.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/sync/model/syncable_service.h"

namespace base {
class DictionaryValue;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PrefService;
class Profile;

class SupervisedUserSyncService : public KeyedService,
                                  public SigninManagerBase::Observer,
                                  public syncer::SyncableService {
 public:
  // For use with GetSupervisedUsersAsync() below.
  typedef base::Callback<void(const base::DictionaryValue*)>
      SupervisedUsersCallback;

  // Dictionary keys for entry values of |prefs::kSupervisedUsers|.
  static const char kAcknowledged[];
  static const char kChromeAvatar[];
  static const char kChromeOsAvatar[];
  static const char kMasterKey[];
  static const char kPasswordSignatureKey[];
  static const char kPasswordEncryptionKey[];
  static const char kName[];

  // Represents a non-existing avatar on Chrome and Chrome OS.
  static const int kNoAvatar;

  ~SupervisedUserSyncService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Extracts the avatar index from the input |avatar_str| and set
  // |avatar_index| to hold the extracted value. Returns true if the
  // index was extracted successfully and false otherwise.
  // |avatar_str| should have the format: "chrome-avatar-index:INDEX"
  // where INDEX is the integer to be extracted. |avatar_str| can be empty
  // in case there is no avatar synced for a supervised user in which case
  // |avatar_index| is set to -1.
  static bool GetAvatarIndex(const std::string& avatar_str,
                             int* avatar_index);

  // Given an |avatar_index|, it returns a string of the form:
  // "chrome-avatar-index:INDEX" where INDEX = |avatar_index|.
  // It is exposed for testing purposes only.
  static std::string BuildAvatarString(int avatar_index);

  void AddObserver(SupervisedUserSyncServiceObserver* observer);
  void RemoveObserver(SupervisedUserSyncServiceObserver* observer);

  void AddSupervisedUser(const std::string& id,
                         const std::string& name,
                         const std::string& master_key,
                         const std::string& signature_key,
                         const std::string& encryption_key,
                         int avatar_index);
  void UpdateSupervisedUser(const std::string& id,
                            const std::string& name,
                            const std::string& master_key,
                            const std::string& signature_key,
                            const std::string& encryption_key,
                            int avatar_index);

  void DeleteSupervisedUser(const std::string& id);

  // Updates the supervised user avatar only if the supervised user has
  // no avatar and |avatar_index| is set to some value other than
  // |kNoAvatar|. If |avatar_index| equals |kNoAvatar| and the
  // supervised user has an avatar, it will be cleared. However,
  // to clear an avatar call the convenience method
  // |ClearSupervisedUserAvatar()|.
  // Returns true if the avatar value is changed (either updated or cleared)
  // and false otherwise.
  bool UpdateSupervisedUserAvatarIfNeeded(const std::string& id,
                                          int avatar_index);
  void ClearSupervisedUserAvatar(const std::string& id);

  // Returns a dictionary containing all supervised users supervised by this
  // custodian. This method should only be called once this service has started
  // syncing supervised users (i.e. has finished its initial merge of local and
  // server-side data, via MergeDataAndStartSyncing), as the stored data might
  // be outdated before that.
  const base::DictionaryValue* GetSupervisedUsers();

  // Calls the passed |callback| with a dictionary containing all supervised
  // users managed by this custodian.
  void GetSupervisedUsersAsync(const SupervisedUsersCallback& callback);

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
  friend class SupervisedUserSyncServiceFactory;

  // Use |SupervisedUserSyncServiceFactory::GetForProfile(...)| to get an
  // instance of this service.
  explicit SupervisedUserSyncService(Profile* profile);

  // SigninManagerBase::Observer implementation.
  void GoogleSignedOut(const std::string& account_id,
                       const std::string& username) override;

  std::unique_ptr<base::DictionaryValue> CreateDictionary(
      const std::string& name,
      const std::string& master_key,
      const std::string& signature_key,
      const std::string& encryption_key,
      int avatar_index);

  void UpdateSupervisedUserImpl(const std::string& id,
                                const std::string& name,
                                const std::string& master_key,
                                const std::string& signature_key,
                                const std::string& encryption_key,
                                int avatar_index,
                                bool add_user);

  void NotifySupervisedUserAcknowledged(const std::string& supervised_user_id);
  void NotifySupervisedUsersSyncingStopped();
  void NotifySupervisedUsersChanged();

  void DispatchCallbacks();

  Profile* profile_;
  PrefService* prefs_;

  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;
  std::unique_ptr<syncer::SyncErrorFactory> error_handler_;

  base::ObserverList<SupervisedUserSyncServiceObserver> observers_;

  std::vector<SupervisedUsersCallback> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserSyncService);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_LEGACY_SUPERVISED_USER_SYNC_SERVICE_H_
