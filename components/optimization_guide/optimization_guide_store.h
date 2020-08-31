// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_STORE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_STORE_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/version.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/optimization_guide/memory_hint.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/store_update_data.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace optimization_guide {
namespace proto {
class StoreEntry;
}  // namespace proto

// The HintCache backing store, which is responsible for storing all hints that
// are locally available. While the HintCache itself may retain some hints in a
// memory cache, all of its hints are initially loaded asynchronously by the
// store. All calls to this store must be made from the same thread.
class OptimizationGuideStore {
 public:
  using HintLoadedCallback =
      base::OnceCallback<void(const std::string&, std::unique_ptr<MemoryHint>)>;
  using PredictionModelLoadedCallback =
      base::OnceCallback<void(std::unique_ptr<proto::PredictionModel>)>;
  using HostModelFeaturesLoadedCallback =
      base::OnceCallback<void(std::unique_ptr<proto::HostModelFeatures>)>;
  using AllHostModelFeaturesLoadedCallback = base::OnceCallback<void(
      std::unique_ptr<std::vector<proto::HostModelFeatures>>)>;
  using EntryKey = std::string;
  using StoreEntryProtoDatabase =
      leveldb_proto::ProtoDatabase<proto::StoreEntry>;

  // Status of the store. The store begins in kUninitialized, transitions to
  // kInitializing after Initialize() is called, and transitions to kAvailable
  // if initialization successfully completes. In the case where anything fails,
  // the store transitions to kFailed, at which point it is fully purged and
  // becomes unusable.
  //
  // Keep in sync with OptimizationGuideHintCacheLevelDBStoreStatus in
  // tools/metrics/histograms/enums.xml.
  enum class Status {
    kUninitialized = 0,
    kInitializing = 1,
    kAvailable = 2,
    kFailed = 3,
    kMaxValue = kFailed,
  };

  // Store entry types within the store appear at the start of the keys of
  // entries. These values are converted into strings within the key: a key
  // starting with "1_" signifies a metadata entry and one starting with "2_"
  // signifies a component hint entry. Adding this to the start of the key
  // allows the store to quickly perform operations on all entries of a specific
  // key type. Given that store entry type comparisons may be performed many
  // times, the entry type string is kept as small as possible.
  //  Example metadata entry type key:
  //   "[StoreEntryType::kMetadata]_[MetadataType::kSchema]"    ==> "1_1"
  //  Example component hint store entry type key:
  //   "[StoreEntryType::kComponentHint]_[component_version]_[host]"
  //     ==> "2_55_foo.com"
  // NOTE: The order and value of the existing store entry types within the enum
  // cannot be changed, but new types can be added to the end.
  // StoreEntryType should remain synchronized with the
  // HintCacheStoreEntryType in enums.xml.
  // Also ensure to add to the OptimizationGuide.StoreEntryTypes histogram
  // suffixes if adding a new one.
  enum class StoreEntryType {
    kEmpty = 0,
    kMetadata = 1,
    kComponentHint = 2,
    kFetchedHint = 3,
    kPredictionModel = 4,
    kHostModelFeatures = 5,
    kMaxValue = kHostModelFeatures,
  };

  OptimizationGuideStore(
      leveldb_proto::ProtoDatabaseProvider* database_provider,
      const base::FilePath& database_dir,
      scoped_refptr<base::SequencedTaskRunner> store_task_runner);
  // For tests only.
  explicit OptimizationGuideStore(
      std::unique_ptr<StoreEntryProtoDatabase> database);
  virtual ~OptimizationGuideStore();

  // Initializes the store. If |purge_existing_data| is set to true,
  // then the cache is purged during initialization and starts in a fresh state.
  // When initialization completes, the provided callback is run asynchronously.
  // Virtualized for testing.
  virtual void Initialize(bool purge_existing_data, base::OnceClosure callback);

  // Creates and returns a StoreUpdateData object for component hints. This
  // object is used to collect hints within a component in a format usable on a
  // background thread and is later returned to the store in
  // UpdateComponentHints(). The StoreUpdateData object is only created when the
  // provided component version is newer than the store's version, indicating
  // fresh hints. If the component's version is not newer than the store's
  // version, then no StoreUpdateData is created and nullptr is returned. This
  // prevents unnecessary processing of the component's hints by the caller.
  std::unique_ptr<StoreUpdateData> MaybeCreateUpdateDataForComponentHints(
      const base::Version& version) const;

  // Creates and returns a HintsUpdateData object for Fetched Hints.
  // This object is used to collect a batch of hints in a format that is usable
  // to update the store on a background thread. This is always created when
  // hints have been successfully fetched from the remote Optimization Guide
  // Service so the store can expire old hints, remove hints specified by the
  // server, and store the fresh hints.
  std::unique_ptr<StoreUpdateData> CreateUpdateDataForFetchedHints(
      base::Time update_time) const;

  // Updates the component hints and version contained within the store. When
  // this is called, all pre-existing component hints within the store is purged
  // and only the new hints are retained. After the store is fully updated with
  // the new component hints, the callback is run asynchronously.
  void UpdateComponentHints(std::unique_ptr<StoreUpdateData> component_data,
                            base::OnceClosure callback);

  // Updates the fetched hints contained in the store, including the
  // metadata entry. The callback is run asynchronously after the database
  // stores the hints.
  void UpdateFetchedHints(std::unique_ptr<StoreUpdateData> fetched_hints_data,
                          base::OnceClosure callback);

  // Removes fetched hint store entries from |this|. |entry_keys_| is
  // updated after the fetched hint entries are removed.
  void ClearFetchedHintsFromDatabase();

  // Finds the most specific hint entry key for the specified host. Returns
  // true if a hint entry key is found, in which case |out_hint_entry_key| is
  // populated with the key. All keys for kFetched hints are considered before
  // kComponent hints as they are updated more frequently.
  bool FindHintEntryKey(const std::string& host,
                        EntryKey* out_hint_entry_key) const;

  // Loads the hint specified by |hint_entry_key|.
  // After the load finishes, the hint data is passed to |callback|. In the case
  // where the hint cannot be loaded, the callback is run with a nullptr.
  // Depending on the load result, the callback may be synchronous or
  // asynchronous.
  void LoadHint(const EntryKey& hint_entry_key, HintLoadedCallback callback);

  // Returns the time that the fetched hints in the store can be updated. If
  // |this| is not available, base::Time() is returned.
  base::Time GetFetchedHintsUpdateTime() const;

  // Removes all fetched hints that have expired from the store.
  // |entry_keys_| is updated after the expired fetched hints are
  // removed.
  void PurgeExpiredFetchedHints();

  // Removes all host model features that have expired from the store.
  // |entry_keys_| is updated after the expired host model features are
  // removed.
  void PurgeExpiredHostModelFeatures();

  // Creates and returns a StoreUpdateData object for Prediction Models. This
  // object is used to collect a batch of prediction models in a format that is
  // usable to update the store on a background thread. This is always created
  // when prediction models have been successfully fetched from the remote
  // Optimization Guide Service so the store can update old prediction models.
  std::unique_ptr<StoreUpdateData> CreateUpdateDataForPredictionModels() const;

  // Updates the prediction models contained in the store. The callback is run
  // asynchronously after the database stores the prediction models. Virtualized
  // for testing.
  virtual void UpdatePredictionModels(
      std::unique_ptr<StoreUpdateData> prediction_models_update_data,
      base::OnceClosure callback);

  // Finds the entry key for the prediction model if it is known to the store.
  // Returns true if an entry key is found and |out_prediction_model_entry_key|
  // is populated with the matching key.
  // Virtualized for testing.
  virtual bool FindPredictionModelEntryKey(
      proto::OptimizationTarget optimization_target,
      OptimizationGuideStore::EntryKey* out_prediction_model_entry_key);

  // Loads the prediction model specified by |prediction_model_entry_key|. After
  // the load finishes, the prediction model data is passed to |callback|. In
  // the case where the prediction model cannot be loaded, the callback is run
  // with a nullptr. Depending on the load result, the callback may be
  // synchronous or asynchronous.
  // Virtualized for testing.
  virtual void LoadPredictionModel(const EntryKey& prediction_model_entry_key,
                                   PredictionModelLoadedCallback callback);

  // Creates and returns a StoreUpdateData object for host model features. This
  // object is used to collect a batch of host model features in a format that
  // is usable to update the store on a background thread. This is always
  // created when host model features have been successfully fetched from the
  // remote Optimization Guide Service so the store can update old host model
  // features.
  std::unique_ptr<StoreUpdateData> CreateUpdateDataForHostModelFeatures(
      base::Time host_model_features_update_time,
      base::Time expiry_time) const;

  // Updates the host model features contained in the store. The callback is run
  // asynchronously after the database stores the host model features.
  // Virtualized for testing.
  virtual void UpdateHostModelFeatures(
      std::unique_ptr<StoreUpdateData> host_model_features_update_data,
      base::OnceClosure callback);

  // Finds the entry key for the host model features for |host| if it is known
  // to the store. Returns true if an entry key is found and
  // |out_host_model_features_entry_key| is populated with the matching key.
  bool FindHostModelFeaturesEntryKey(
      const std::string& host,
      OptimizationGuideStore::EntryKey* out_host_model_features_entry_key)
      const;

  // Loads the host model features specified by |host_model_features_entry_key|.
  // After the load finishes, the host model features data is passed to
  // |callback|. In the case where the host model features cannot be loaded, the
  // callback is run with a nullptr. Depending on the load result, the callback
  // may be synchronous or asynchronous.
  void LoadHostModelFeatures(const EntryKey& host_model_features_entry_key,
                             HostModelFeaturesLoadedCallback callback);

  // Loads all the host model features known to the store. After the load
  // finishes, the host model features data is passed back to |callback|. In the
  // case where the host model features cannot be loaded, the callback is run
  // with a nullptr. Depending on the load result, the callback may be
  // synchronous or asynchronous.
  // Virtualized for testing.
  virtual void LoadAllHostModelFeatures(
      AllHostModelFeaturesLoadedCallback callback);

  // Returns the time that the host model features in the store can be updated.
  // If |this| is not available, base::Time() is returned.
  base::Time GetHostModelFeaturesUpdateTime() const;

  // Clears all host model features from the database and resets the entry keys.
  void ClearHostModelFeaturesFromDatabase();

 private:
  friend class OptimizationGuideStoreTest;
  friend class StoreUpdateData;
  friend class TestOptimizationGuideStore;

  using EntryKeyPrefix = std::string;
  using EntryKeySet = base::flat_set<EntryKey>;

  using EntryVector =
      leveldb_proto::ProtoDatabase<proto::StoreEntry>::KeyEntryVector;
  using EntryMap = std::map<EntryKey, proto::StoreEntry>;

  // Metadata types within the store. The metadata type appears at the end of
  // metadata entry keys. These values are converted into strings within the
  // key.
  //  Example metadata type keys:
  //   "[StoreEntryType::kMetadata]_[MetadataType::kSchema]"    ==> "1_1"
  //   "[StoreEntryType::kMetadata]_[MetadataType::kComponent]" ==> "1_2"
  // NOTE: The order and value of the existing metadata types within the enum
  // cannot be changed, but new types can be added to the end.
  enum class MetadataType {
    kSchema = 1,
    kComponent = 2,
    kFetched = 3,
    kHostModelFeatures = 4,
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

  // Returns prefix of the key of every fetched hint entry: "3_".
  static EntryKeyPrefix GetFetchedHintEntryKeyPrefix();

  // Returns prefix of the key of every prediction model entry: "4_".
  static EntryKeyPrefix GetPredictionModelEntryKeyPrefix();

  // Returns prefix of the key of every host model features entry: "5_".
  static EntryKeyPrefix GetHostModelFeaturesEntryKeyPrefix();

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

  // Asynchronously loads the entry keys from the store, populates |entry_keys_|
  // with them, and runs the provided callback after they finish loading. In the
  // case where there is currently an in-flight update, this does nothing, as
  // the entry keys will be loaded after the update completes.
  void MaybeLoadEntryKeys(base::OnceClosure callback);

  // Returns the total entry keys contained within the store.
  size_t GetEntryKeyCount() const;

  // Finds the most specific host suffix of the host name that the store has an
  // entry with the provided prefix, |entry_key_prefix|. |out_entry_key| is
  // populated with the entry key for the corresponding hint. Returns true if
  // an entry was successfully found.
  bool FindEntryKeyForHostWithPrefix(
      const std::string& host,
      EntryKey* out_entry_key,
      const EntryKeyPrefix& entry_key_prefix) const;

  // Callback that identifies any expired |entries| and
  // asynchronously removes them from the store.
  void OnLoadEntriesToPurgeExpired(bool success,
                                   std::unique_ptr<EntryMap> entries);

  // Callback that runs after the database finishes being initialized. If
  // |purge_existing_data| is true, then unconditionally purges the database;
  // otherwise, triggers loading of the metadata.
  void OnDatabaseInitialized(bool purge_existing_data,
                             base::OnceClosure callback,
                             leveldb_proto::Enums::InitStatus status);

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

  // Callback that runs after the data within the store is fully
  // updated. If the update was successful, it attempts to load all of the
  // entry keys contained within the database.
  void OnUpdateStore(base::OnceClosure callback, bool success);

  // Callback that runs after the hint entry keys are fully loaded. If there's
  // currently an in-flight component update, then the hint entry keys will be
  // loaded again after the component update completes, so the results are
  // tossed; otherwise, |entry_keys| is moved into |entry_keys_|.
  // Regardless of the outcome of loading the keys, the callback always runs.
  void OnLoadEntryKeys(std::unique_ptr<EntryKeySet> entry_keys,
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
                  std::unique_ptr<proto::StoreEntry> entry);

  // Callback that runs after a prediction model entry is loaded from the
  // database. If there's currently an in-flight update, then the data could be
  // invalidated, so loaded model is discarded. Otherwise, the prediction model
  // is released into the callback, allowing the caller to own the prediction
  // model without copying it. Regardless of the success or failure of
  // retrieving the key, the callback always runs (it simply runs with a nullptr
  // on failure).
  void OnLoadPredictionModel(PredictionModelLoadedCallback callback,
                             bool success,
                             std::unique_ptr<proto::StoreEntry> entry);

  // Callback that runs after a host model features entry is loaded from the
  // database. If there's currently an in-flight update, then the data could be
  // invalidated, so loaded host model features data is discarded. Otherwise,
  // the host model features are released into the callback, allowing the caller
  // to own the host model features without copying it. Regardless of the
  // success or failure of retrieving the key, the callback always runs (it
  // simply runs with a nullptr on failure).
  void OnLoadHostModelFeatures(HostModelFeaturesLoadedCallback callback,
                               bool success,
                               std::unique_ptr<proto::StoreEntry> entry);

  // Callback that runs after all the host model features entries are loaded
  // from the database. If there's currently an in-flight update, then the data
  // could be invalidated, so loaded host model features data is discarded.
  // Otherwise, the host model features are released into the callback, allowing
  // the caller to own the host model features without copying it. Regardless of
  // the success or failure of retrieving the key, the callback always runs (it
  // simply runs with a nullptr on failure).
  void OnLoadAllHostModelFeatures(
      AllHostModelFeaturesLoadedCallback callback,
      bool success,
      std::unique_ptr<std::vector<proto::StoreEntry>> entry);

  // Proto database used by the store.
  std::unique_ptr<StoreEntryProtoDatabase> database_;

  // The current status of the store. It should only be updated through
  // UpdateStatus(), which validates status transitions and triggers
  // accompanying logic.
  Status status_ = Status::kUninitialized;

  // The current component version of the store. This should only be updated
  // via SetComponentVersion(), which ensures that both |component_version_|
  // and |component_hint_key_prefix_| are updated at the same time.
  base::Optional<base::Version> component_version_;

  // The current entry key prefix shared by all component hints containd within
  // the store. While this could be generated on the fly using
  // |component_version_|, it is retaind separately as an optimization, as it
  // is needed often.
  EntryKeyPrefix component_hint_entry_key_prefix_;

  // The next update time for the fetched hints that are currently in the
  // store.
  base::Time fetched_update_time_;

  // The next update time for the host model features that are currently in the
  // store.
  base::Time host_model_features_update_time_;

  // The keys of the entries available within the store.
  std::unique_ptr<EntryKeySet> entry_keys_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OptimizationGuideStore> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideStore);
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_STORE_H_
