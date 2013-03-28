// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Wraps PrefService in an InvalidationStateTracker to allow SyncNotifiers
// to use PrefService as persistence for invalidation state. It is not thread
// safe, and lives on the UI thread.

#ifndef CHROME_BROWSER_SYNC_INVALIDATIONS_INVALIDATOR_STORAGE_H_
#define CHROME_BROWSER_SYNC_INVALIDATIONS_INVALIDATOR_STORAGE_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "sync/notifier/invalidation_state_tracker.h"

class PrefService;
class PrefRegistrySyncable;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace browser_sync {

// TODO(tim): Bug 124137. We may want to move this outside of sync/ into a
// browser/invalidations directory, or re-organize to have a browser
// subdirectory that contains signin/ sync/ invalidations/ and other cloud
// services.  For now this is still tied to sync while we refactor, so minimize
// churn and keep it here.
class InvalidatorStorage : public base::SupportsWeakPtr<InvalidatorStorage>,
                           public syncer::InvalidationStateTracker {
 public:
  static void RegisterUserPrefs(PrefRegistrySyncable* registry);

  // |pref_service| may be NULL (for unit tests), but in that case no setter
  // methods should be called. Does not own |pref_service|.
  explicit InvalidatorStorage(PrefService* pref_service);
  virtual ~InvalidatorStorage();

  // InvalidationStateTracker implementation.
  virtual syncer::InvalidationStateMap GetAllInvalidationStates() const
      OVERRIDE;
  virtual void SetMaxVersionAndPayload(const invalidation::ObjectId& id,
                                       int64 max_version,
                                       const std::string& payload) OVERRIDE;
  virtual void Forget(const syncer::ObjectIdSet& ids) OVERRIDE;
  virtual void SetInvalidatorClientId(const std::string& client_id) OVERRIDE;
  virtual std::string GetInvalidatorClientId() const OVERRIDE;
  virtual void SetBootstrapData(const std::string& data) OVERRIDE;
  virtual std::string GetBootstrapData() const OVERRIDE;
  virtual void Clear() OVERRIDE;
  virtual void GenerateAckHandles(
      const syncer::ObjectIdSet& ids,
      const scoped_refptr<base::TaskRunner>& task_runner,
      base::Callback<void(const syncer::AckHandleMap&)> callback) OVERRIDE;
  virtual void Acknowledge(const invalidation::ObjectId& id,
                           const syncer::AckHandle& ack_handle) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(InvalidatorStorageTest, SerializeEmptyMap);
  FRIEND_TEST_ALL_PREFIXES(InvalidatorStorageTest,
                           DeserializeFromListOutOfRange);
  FRIEND_TEST_ALL_PREFIXES(InvalidatorStorageTest,
                           DeserializeFromListInvalidFormat);
  FRIEND_TEST_ALL_PREFIXES(InvalidatorStorageTest,
                           DeserializeFromListWithDuplicates);
  FRIEND_TEST_ALL_PREFIXES(InvalidatorStorageTest,
                           DeserializeFromEmptyList);
  FRIEND_TEST_ALL_PREFIXES(InvalidatorStorageTest, DeserializeFromListBasic);
  FRIEND_TEST_ALL_PREFIXES(InvalidatorStorageTest,
                           DeserializeFromListMissingOptionalValues);
  FRIEND_TEST_ALL_PREFIXES(InvalidatorStorageTest, DeserializeMapOutOfRange);
  FRIEND_TEST_ALL_PREFIXES(InvalidatorStorageTest, DeserializeMapInvalidFormat);
  FRIEND_TEST_ALL_PREFIXES(InvalidatorStorageTest,
                           DeserializeMapEmptyDictionary);
  FRIEND_TEST_ALL_PREFIXES(InvalidatorStorageTest, DeserializeMapBasic);
  FRIEND_TEST_ALL_PREFIXES(InvalidatorStorageTest, MigrateLegacyPreferences);

  base::ThreadChecker thread_checker_;

  // Helpers to convert between InvalidationStateMap <--> ListValue.
  static void DeserializeFromList(
      const base::ListValue& state_map_list,
      syncer::InvalidationStateMap* state_map);
  static void SerializeToList(
      const syncer::InvalidationStateMap& state_map,
      base::ListValue* state_map_list);

  // Code for migrating from old MaxInvalidationVersions pref, which was a map
  // from sync types to max invalidation versions.
  void MigrateMaxInvalidationVersionsPref();
  static void DeserializeMap(const base::DictionaryValue* max_versions_dict,
                             syncer::InvalidationStateMap* map);

  // May be NULL.
  PrefService* const pref_service_;

  DISALLOW_COPY_AND_ASSIGN(InvalidatorStorage);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_INVALIDATIONS_INVALIDATOR_STORAGE_H_
