// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MANAGED_MODE_POLICY_PROVIDER_H_
#define CHROME_BROWSER_POLICY_MANAGED_MODE_POLICY_PROVIDER_H_

#include "base/memory/ref_counted.h"
#include "base/prefs/pref_store.h"
#include "chrome/browser/policy/configuration_policy_provider.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/syncable_service.h"

class PersistentPrefStore;
class Profile;

namespace base {
class SequencedTaskRunner;
}

namespace policy {

// This class syncs managed user settings from a server, which are mapped to
// policies. The downloaded settings are persisted in a PrefStore.
// Settings are key-value pairs. The key uniquely identifies the setting, and
// in most cases corresponds to a policy key. The value is a string containing
// a JSON serialization of an arbitrary value, which is the value of the policy.
//
// There are two kinds of settings handled by this class: Atomic and split
// settings.
// Atomic settings consist of a single key (that corresponds to a policy name)
// and a single (arbitrary) value.
// Split settings encode a dictionary value and are stored as multiple Sync
// items, one for each dictionary entry. The key for each of these Sync items
// is the key of the split setting, followed by a separator (':') and the key
// for the dictionary entry. The value of the Sync item is the value of the
// dictionary entry.
//
// As an example, a split setting with key "Moose" and value
//   {
//     "foo": "bar",
//     "baz": "blurp"
//   }
// would be encoded as two sync items, one with key "Moose:foo" and value "bar",
// and one with key "Moose:baz" and value "blurp".
//
// TODO(bauerb): This should be split up into a SyncableService and a
// ConfigurationPolicyProvider.
class ManagedModePolicyProvider
    : public ConfigurationPolicyProvider,
      public PrefStore::Observer,
      public syncer::SyncableService {
 public:
  // Creates a new ManagedModePolicyProvider that caches its policies in a JSON
  // file inside the profile folder. |sequenced_task_runner| ensures that all
  // file I/O operations are executed in the order that does not collide
  // with Profile's file operations. If |force_immediate_policy_load| is true,
  // then the underlying policies are loaded immediately before this call
  // returns, otherwise they will be loaded asynchronously in the background.
  static scoped_ptr<ManagedModePolicyProvider> Create(
      Profile* profile,
      base::SequencedTaskRunner* sequenced_task_runner,
      bool force_immediate_policy_load);

  // Use this constructor to inject a different PrefStore (e.g. for testing).
  explicit ManagedModePolicyProvider(PersistentPrefStore* store);
  virtual ~ManagedModePolicyProvider();

  // Sets all local policies, i.e. policies that will not be configured via
  // the server.
  void InitLocalPolicies();

  // Clears all managed user settings and items.
  void Clear();

  // Constructs a key for a split managed user setting from a prefix and a
  // variable key.
  static std::string MakeSplitSettingKey(const std::string& prefix,
                                         const std::string& key);

  // Uploads an item to the Sync server. Items are the same data structure as
  // managed user settings (i.e. key-value pairs, as described at the top of
  // the file), but they are only uploaded (whereas managed user settings are
  // only downloaded), and never passed to the policy system.
  // An example of an uploaded item is an access request to a blocked URL.
  void UploadItem(const std::string& key, scoped_ptr<Value> value);

  // Sets the policy with the given |key| to a copy of the given |value|.
  // Note that policies are updated asynchronously, so the policy won't take
  // effect immediately after this method.
  void SetLocalPolicyForTesting(const std::string& key,
                                scoped_ptr<base::Value> value);

  // Public for testing.
  static syncer::SyncData CreateSyncDataForPolicy(const std::string& name,
                                                  const Value* value);

  // ConfigurationPolicyProvider implementation:
  virtual void Shutdown() OVERRIDE;
  virtual void RefreshPolicies() OVERRIDE;
  virtual bool IsInitializationComplete(PolicyDomain domain) const OVERRIDE;

  // PrefStore::Observer implementation:
  virtual void OnPrefValueChanged(const std::string& key) OVERRIDE;
  virtual void OnInitializationCompleted(bool success) OVERRIDE;

  // SyncableService implementation:
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

 private:
  base::DictionaryValue* GetOrCreateDictionary(const std::string& key) const;
  base::DictionaryValue* GetAtomicSettings() const;
  base::DictionaryValue* GetSplitSettings() const;
  base::DictionaryValue* GetQueuedItems() const;

  // Returns the dictionary where a given Sync item should be stored, depending
  // on whether the managed user setting is atomic or split. In case of a split
  // setting, the split setting prefix of |key| is removed, so that |key| can
  // be used to update the returned dictionary.
  DictionaryValue* GetDictionaryAndSplitKey(std::string* key) const;
  void UpdatePolicyFromCache();

  // Used for persisting policies. Unlike other PrefStores, this one is not
  // hooked up to the PrefService.
  scoped_refptr<PersistentPrefStore> store_;

  // A set of local policies that are fixed and not configured remotely.
  scoped_ptr<base::DictionaryValue> local_policies_;

  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;
  scoped_ptr<syncer::SyncErrorFactory> error_handler_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_MANAGED_MODE_POLICY_PROVIDER_H_
