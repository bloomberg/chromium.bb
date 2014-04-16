// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Wraps PrefService in an InvalidationStateTracker to allow SyncNotifiers
// to use PrefService as persistence for invalidation state. It is not thread
// safe, and lives on the UI thread.

#ifndef CHROME_BROWSER_INVALIDATION_INVALIDATOR_STORAGE_H_
#define CHROME_BROWSER_INVALIDATION_INVALIDATOR_STORAGE_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/threading/thread_checker.h"
#include "sync/notifier/invalidation_state_tracker.h"
#include "sync/notifier/unacked_invalidation_set.h"

class PrefService;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace invalidation {

class InvalidatorStorage : public syncer::InvalidationStateTracker {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // |pref_service| may not be NULL. Does not own |pref_service|.
  explicit InvalidatorStorage(PrefService* pref_service);
  virtual ~InvalidatorStorage();

  // InvalidationStateTracker implementation.
  virtual void ClearAndSetNewClientId(const std::string& client_id) OVERRIDE;
  virtual std::string GetInvalidatorClientId() const OVERRIDE;
  virtual void SetBootstrapData(const std::string& data) OVERRIDE;
  virtual std::string GetBootstrapData() const OVERRIDE;
  virtual void SetSavedInvalidations(
      const syncer::UnackedInvalidationsMap& map) OVERRIDE;
  virtual syncer::UnackedInvalidationsMap GetSavedInvalidations()
      const OVERRIDE;
  virtual void Clear() OVERRIDE;

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

  PrefService* const pref_service_;

  DISALLOW_COPY_AND_ASSIGN(InvalidatorStorage);
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_INVALIDATOR_STORAGE_H_
