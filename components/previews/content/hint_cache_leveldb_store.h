// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_HINT_CACHE_LEVELDB_STORE_H_
#define COMPONENTS_PREVIEWS_CONTENT_HINT_CACHE_LEVELDB_STORE_H_

#include "components/previews/content/hint_cache_store.h"

#include <map>
#include <string>
#include <unordered_set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/previews/content/proto/hint_cache.pb.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace previews {

// Concrete implementation of HintCacheStore using LevelDB. All public calls in
// the store must be made on the same thread.
class HintCacheLevelDBStore : public HintCacheStore {
 public:
  using StoreEntryProtoDatabase =
      leveldb_proto::ProtoDatabase<previews::proto::StoreEntry>;

  // Status of the store. The store begins in kUninitialized, transitions to
  // kInitializing after Initialize() is called, and transitions to kAvailable
  // if initialization successfully completes. In the case where anything fails,
  // the store transitions to kFailed, at which point it is fully purged and
  // becomes unusable.
  //
  // Keep in sync with PreviewsHintCacheLevelDBStoreStatus in
  // tools/metrics/histograms/enums.xml.
  enum class Status {
    kUninitialized = 0,
    kInitializing = 1,
    kAvailable = 2,
    kFailed = 3,
    kMaxValue = kFailed,
  };

  HintCacheLevelDBStore(
      const base::FilePath& database_dir,
      scoped_refptr<base::SequencedTaskRunner> store_task_runner);
  HintCacheLevelDBStore(const base::FilePath& database_dir,
                        std::unique_ptr<StoreEntryProtoDatabase> database);
  ~HintCacheLevelDBStore() override;

  // HintCacheStore overrides:
  void Initialize(bool purge_existing_data,
                  base::OnceClosure callback) override;

  std::unique_ptr<ComponentUpdateData> MaybeCreateComponentUpdateData(
      const base::Version& version) const override;

  void UpdateComponentData(std::unique_ptr<ComponentUpdateData> component_data,
                           base::OnceClosure callback) override;

  bool FindHintEntryKey(const std::string& host_suffix,
                        EntryKey* out_hint_entry_key) const override;

  void LoadHint(const EntryKey& hint_entry_key,
                HintLoadedCallback callback) override;

 private:
  friend class HintCacheLevelDBStoreTest;

  using EntryKeyPrefix = std::string;
  using EntryKeySet = std::unordered_set<EntryKey>;

  using EntryVector =
      leveldb_proto::ProtoDatabase<previews::proto::StoreEntry>::KeyEntryVector;
  using EntryMap = std::map<EntryKey, previews::proto::StoreEntry>;

  // Entry types within the store appear at the start of the keys of entries.
  // These values are converted into strings within the key: a key starting with
  // "1_" signifies a metadata entry and one starting with "2_" signifies a
  // component hint entry. Adding this to the start of the key allows the store
  // to quickly perform operations on all entries of a specific key type. Given
  // that entry type comparisons may be performed many times, the entry type
  // string is kept as small as possible.
  //  Example metadata entry type key:
  //   "[EntryType::kMetadata]_[MetadataType::kSchema]"    ==> "1_1"
  //  Example component hint entry type key:
  //   "[EntryType::kComponentHint]_[component_version]_[host]"
  //     ==> "2_55_foo.com"
  // NOTE: The order and value of the existing entry types within the enum
  // cannot be changed, but new types can be added to the end.
  enum class EntryType {
    kMetadata = 1,
    kComponentHint = 2,
  };

  // Metadata types within the store. The metadata type appears at the end of
  // metadata entry keys. These values are converted into strings within the
  // key.
  //  Example metadata type keys:
  //   "[EntryType::kMetadata]_[MetadataType::kSchema]"    ==> "1_1"
  //   "[EntryType::kMetadata]_[MetadataType::kComponent]" ==> "1_2"
  // NOTE: The order and value of the existing metadata types within the enum
  // cannot be changed, but new types can be added to the end.
  enum class MetadataType {
    kSchema = 1,
    kComponent = 2,
  };

  // HintCacheLevelDBStore's concrete implementation of ComponentUpdateData.
  // LevelDBComponentUpdateData is private within HintCacheLevelDBStore. All
  // classes outside of HintCacheLevelDBStore can only interact with the
  // ComponentUpdateData base class. LevelDBComponentUpdateData is created by
  // HintCacheLevelDBStore when MaybeCreateComponentUpdateData() is called and
  // used to update the store's component data during UpdateComponentData().
  class LevelDBComponentUpdateData : public ComponentUpdateData {
   public:
    explicit LevelDBComponentUpdateData(const base::Version& version);
    ~LevelDBComponentUpdateData() override;

    // ComponentUpdateData overrides:
    void MoveHintIntoUpdateData(
        optimization_guide::proto::Hint&& hint) override;

   private:
    friend class HintCacheLevelDBStore;

    // The prefix to add to the key of every component hint entry. It is set
    // during construction, using the provided component version.
    const EntryKeyPrefix component_hint_entry_key_prefix_;

    // The vector of entries to save. This contains both the metadata component
    // entry, which is created during construction, using the provided component
    // version, and the hint entries from the component, which are individually
    // moved into |entries_to_save_| during calls to MoveHintIntoUpdateData().
    std::unique_ptr<EntryVector> entries_to_save_;
  };

  // Current schema version of the hint cache store. When this is changed,
  // pre-existing store data from an earlier version is purged.
  static const char kStoreSchemaVersion[];

  // Returns prefix in the key of every metadata entry type entry: "1_"
  static EntryKeyPrefix GetMetadataEntryKeyPrefix();

  // Returns entry key for the specified metadata type entry: "1_[MetadataType]"
  static EntryKey GetMetadataTypeEntryKey(MetadataType metadata_type);

  // Returns prefix in the key of every component hint entry: "2_"
  static EntryKeyPrefix GetComponentHintEntryKeyPrefixWithoutVersion();

  // Returns prefix in the key of component hint entries for the specified
  // component version: "2_[component_version]_"
  static EntryKeyPrefix GetComponentHintEntryKeyPrefix(
      const base::Version& component_version);

  // Updates the status of the store to the specified value, validates the
  // transition, and destroys the database in the case where the status
  // transitions to Status::kFailed.
  void UpdateStatus(Status new_status);

  // Returns true if the current status is Status::kAvailable.
  bool IsAvailable() const;

  // Asynchronously purges all existing entries from the database and runs the
  // callback after it completes. This should only be run during initialization.
  void PurgeDatabase(base::OnceClosure callback);

  // Updates |component_version_| and |component_hint_entry_key_prefix_| for
  // the new component version. This must be called rather than directly
  // modifying |component_version_|, as it ensures that both member variables
  // are updated in sync.
  void SetComponentVersion(const base::Version& component_version);

  // Resets |component_version_| and |component_hint_entry_key_prefix_| back to
  // their default state. Called after the database is destroyed.
  void ClearComponentVersion();

  // Asynchronously loads the hint entry keys from the store, populates
  // |hint_entry_keys_| with them, and runs the provided callback after they
  // finish loading. In the case where there is currently an in-flight component
  // update, this does nothing, as the hint entry keys will be loaded after the
  // component update completes.
  void MaybeLoadHintEntryKeys(base::OnceClosure callback);

  // Returns the total hint entry keys contained within the store.
  size_t GetHintEntryKeyCount() const;

  // Callback that runs after the database finishes being initialized. If
  // |purge_existing_data| is true, then unconditionally purges the database;
  // otherwise, triggers loading of the metadata.
  void OnDatabaseInitialized(bool purge_existing_data,
                             base::OnceClosure callback,
                             bool success);

  // Callback that is run after the database finishes being destroyed.
  void OnDatabaseDestroyed(bool success);

  // Callback that runs after the metadata finishes being loaded. This
  // validates the schema version, sets the component version, and either purges
  // the store (on a schema version mismatch) or loads all hint entry keys (on
  // a schema version match).
  void OnLoadMetadata(base::OnceClosure callback,
                      bool success,
                      std::unique_ptr<EntryMap> metadata_entries);

  // Callback that runs after the database is purged during initialization.
  void OnPurgeDatabase(base::OnceClosure callback, bool success);

  // Callback that runs after the component data within the store is fully
  // updated. If the update was successful, it attempts to load all of the hint
  // entry keys contained within the database.
  void OnUpdateComponentData(base::OnceClosure callback, bool success);

  // Callback that runs after the hint entry keys are fully loaded. If there's
  // currently an in-flight component update, then the hint entry keys will be
  // loaded again after the component update completes, so the results are
  // tossed; otherwise, |hint_entry_keys| is moved into |hint_entry_keys_|.
  // Regardless of the outcome of loading the keys, the callback always runs.
  void OnLoadHintEntryKeys(std::unique_ptr<EntryKeySet> hint_entry_keys,
                           base::OnceClosure callback,
                           bool success,
                           std::unique_ptr<EntryMap> unused);

  // Callback that runs after a hint entry is loaded from the database. If
  // there's currently an in-flight component update, then the hint is about to
  // be invalidated, so results are tossed; otherwise, the hint is released into
  // the callback, allowing the caller to own the hint without copying it.
  // Regardless of the success or failure of retrieving the key, the callback
  // always runs (it simply runs with a nullptr on failure).
  void OnLoadHint(const EntryKey& entry_key,
                  HintLoadedCallback callback,
                  bool success,
                  std::unique_ptr<previews::proto::StoreEntry> entry);

  // Path to the directory for the profile using this store.
  const base::FilePath database_dir_;

  // Proto database used by the store.
  const std::unique_ptr<StoreEntryProtoDatabase> database_;

  // The current status of the store. It should only be updated through
  // UpdateStatus(), which validates status transitions and triggers
  // accompanying logic.
  Status status_;

  // The current component version of the store. This should only be updated
  // via SetComponentVersion(), which ensures that both |component_version_|
  // and |component_hint_key_prefix_| are updated at the same time.
  base::Optional<base::Version> component_version_;
  // The current entry key prefix shared by all component hints containd within
  // the store. While this could be generated on the fly using
  // |component_version_|, it is retaind separately as an optimization, as it
  // is needed often.
  EntryKeyPrefix component_hint_entry_key_prefix_;

  // If a component data update is in the middle of being processed; when this
  // is true, keys and hints will not be returned by the store.
  bool component_data_update_in_flight_;

  // The keys of the hints available within the store.
  std::unique_ptr<EntryKeySet> hint_entry_keys_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<HintCacheLevelDBStore> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HintCacheLevelDBStore);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_HINT_CACHE_LEVELDB_STORE_H_
