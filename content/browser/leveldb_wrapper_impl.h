// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LEVELDB_WRAPPER_IMPL_H_
#define CONTENT_BROWSER_LEVELDB_WRAPPER_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "content/common/leveldb_wrapper.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}

namespace content {

// This is a wrapper around a leveldb::mojom::LevelDBDatabase. Multiple
// interface
// pointers can be bound to the same object. The wrapper adds a couple of
// features not found directly in leveldb.
// 1) Adds the given prefix, if any, to all keys. This allows the sharing of one
//    database across many, possibly untrusted, consumers and ensuring that they
//    can't access each other's values.
// 2) Enforces a max_size constraint.
// 3) Informs observers when values scoped by prefix are modified.
// 4) Throttles requests to avoid overwhelming the disk.
class CONTENT_EXPORT LevelDBWrapperImpl : public mojom::LevelDBWrapper {
 public:
  using ValueMap = std::map<std::vector<uint8_t>, std::vector<uint8_t>>;
  using ValueMapCallback = base::OnceCallback<void(std::unique_ptr<ValueMap>)>;
  using Change =
      std::pair<std::vector<uint8_t>, base::Optional<std::vector<uint8_t>>>;
  using KeysOnlyMap = std::map<std::vector<uint8_t>, size_t>;

  class CONTENT_EXPORT Delegate {
   public:
    virtual void OnNoBindings() = 0;
    virtual std::vector<leveldb::mojom::BatchedOperationPtr>
    PrepareToCommit() = 0;
    virtual void DidCommit(leveldb::mojom::DatabaseError error) = 0;
    // Called during loading if no data was found. Needs to call |callback|.
    virtual void MigrateData(ValueMapCallback callback);
    // Called during loading to give delegate a chance to modify the data as
    // stored in the database.
    virtual std::vector<Change> FixUpData(const ValueMap& data);
    virtual void OnMapLoaded(leveldb::mojom::DatabaseError error);
  };

  enum class CacheMode {
    // The cache stores only keys (required to maintain max size constraints)
    // when there is only one client binding to save memory. The client is
    // asked to send old values on mutations for sending notifications to
    // observers.
    KEYS_ONLY_WHEN_POSSIBLE,
    // The cache always stores keys and values.
    KEYS_AND_VALUES
  };

  // Options provided to constructor.
  struct Options {
    CacheMode cache_mode = CacheMode::KEYS_AND_VALUES;

    // Max bytes of storage that can be used by key value pairs.
    size_t max_size = 0;
    // Minimum time between 2 commits to disk.
    base::TimeDelta default_commit_delay;
    // Maximum number of bytes written to disk in one hour.
    int max_bytes_per_hour = 0;
    // Maximum number of disk write batches in one hour.
    int max_commits_per_hour = 0;
  };

  // |no_bindings_callback| will be called when this object has no more
  // bindings and all pending modifications have been processed.
  LevelDBWrapperImpl(leveldb::mojom::LevelDBDatabase* database,
                     const std::string& prefix,
                     Delegate* delegate,
                     const Options& options);
  ~LevelDBWrapperImpl() override;

  void Bind(mojom::LevelDBWrapperRequest request);

  // The total bytes used by items which counts towards the quota.
  size_t storage_used() const { return storage_used_; }
  // The physical memory used by the cache.
  size_t memory_used() const { return memory_used_; }
  bool empty() const { return storage_used_ == 0; }

  bool has_pending_load_tasks() const {
    return !on_load_complete_tasks_.empty();
  }

  // Commence aggressive flushing. This should be called early during startup,
  // before any localStorage writing. Currently scheduled writes will not be
  // rescheduled and will be flushed at the scheduled time after which
  // aggressive flushing will commence.
  static void EnableAggressiveCommitDelay();

  // Commits any uncommitted data to the database as soon as possible. This
  // usually means data will be committed immediately, but if we're currently
  // waiting on the result of initializing our map the commit won't happen
  // until the load has finished.
  void ScheduleImmediateCommit();

  // Clears the in-memory cache if currently no changes are pending. If there
  // are uncommitted changes this method does nothing.
  void PurgeMemory();

  // Adds memory statistics to |pmd| for memory infra.
  void OnMemoryDump(const std::string& name,
                    base::trace_event::ProcessMemoryDump* pmd);

  // Sets cache mode to either store only keys or keys and values. See
  // SetCacheMode().
  void SetCacheModeForTesting(CacheMode cache_mode);

  // LevelDBWrapper:
  void AddObserver(mojom::LevelDBObserverAssociatedPtrInfo observer) override;
  void Put(const std::vector<uint8_t>& key,
           const std::vector<uint8_t>& value,
           const base::Optional<std::vector<uint8_t>>& client_old_value,
           const std::string& source,
           PutCallback callback) override;
  void Delete(const std::vector<uint8_t>& key,
              const base::Optional<std::vector<uint8_t>>& client_old_value,
              const std::string& source,
              DeleteCallback callback) override;
  void DeleteAll(const std::string& source,
                 DeleteAllCallback callback) override;
  void Get(const std::vector<uint8_t>& key, GetCallback callback) override;
  void GetAll(
      mojom::LevelDBWrapperGetAllCallbackAssociatedPtrInfo complete_callback,
      GetAllCallback callback) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(LevelDBWrapperImplTest, SetOnlyKeysWithoutDatabase);
  FRIEND_TEST_ALL_PREFIXES(LevelDBWrapperImplTest, CommitOnDifferentCacheModes);
  FRIEND_TEST_ALL_PREFIXES(LevelDBWrapperImplTest, GetAllWhenCacheOnlyKeys);
  FRIEND_TEST_ALL_PREFIXES(LevelDBWrapperImplTest, GetAllAfterSetCacheMode);
  FRIEND_TEST_ALL_PREFIXES(LevelDBWrapperImplTest, SetCacheModeConsistent);

  // Used to rate limit commits.
  class RateLimiter {
   public:
    RateLimiter(size_t desired_rate, base::TimeDelta time_quantum);

    void add_samples(size_t samples) { samples_ += samples;  }

    // Computes the total time needed to process the total samples seen
    // at the desired rate.
    base::TimeDelta ComputeTimeNeeded() const;

    // Given the elapsed time since the start of the rate limiting session,
    // computes the delay needed to mimic having processed the total samples
    // seen at the desired rate.
    base::TimeDelta ComputeDelayNeeded(
        const base::TimeDelta elapsed_time) const;

   private:
    float rate_;
    float samples_;
    base::TimeDelta time_quantum_;
  };

  enum class LoadState { UNLOADED = 0, KEYS_ONLY, KEYS_AND_VALUES };

  struct CommitBatch {
    bool clear_all_first;
    // Values in this map are only used if |keys_only_map_| is loaded.
    std::map<std::vector<uint8_t>, base::Optional<std::vector<uint8_t>>>
        changed_values;

    CommitBatch();
    ~CommitBatch();
  };

  // Changes the cache (a copy of map stored in memory) to contain only keys
  // instead of keys and values. The only keys mode can be set only when there
  // is one client binding and it automatically changes to keys and values mode
  // when more than one binding exists. The max size constraints are still
  // valid. Notification to the observers when an item is mutated depends on the
  // |client_old_value| when cache contains only keys. Changing the mode and
  // using GetAll() when only keys are stored would cause extra disk access.
  void SetCacheMode(CacheMode cache_mode);

  void OnConnectionError();
  void LoadMap(const base::Closure& completion_callback);
  void OnMapLoaded(leveldb::mojom::DatabaseError status,
                   std::vector<leveldb::mojom::KeyValuePtr> data);
  void OnGotMigrationData(std::unique_ptr<ValueMap> data);
  void OnLoadComplete();
  void CreateCommitBatchIfNeeded();
  void StartCommitTimer();
  base::TimeDelta ComputeCommitDelay() const;
  void CommitChanges();
  void OnCommitComplete(leveldb::mojom::DatabaseError error);
  void UnloadMapIfPossible();
  bool IsMapReloadNeeded() const;
  LoadState CurrentLoadState() const;

  std::vector<uint8_t> prefix_;
  mojo::BindingSet<mojom::LevelDBWrapper> bindings_;
  mojo::AssociatedInterfacePtrSet<mojom::LevelDBObserver> observers_;
  Delegate* delegate_;
  leveldb::mojom::LevelDBDatabase* database_;
  std::unique_ptr<ValueMap> keys_values_map_;
  std::unique_ptr<KeysOnlyMap> keys_only_map_;
  LoadState desired_load_state_;
  std::vector<base::Closure> on_load_complete_tasks_;
  size_t storage_used_;
  size_t max_size_;
  size_t memory_used_;
  base::TimeTicks start_time_;
  base::TimeDelta default_commit_delay_;
  RateLimiter data_rate_limiter_;
  RateLimiter commit_rate_limiter_;
  int commit_batches_in_flight_ = 0;
  std::unique_ptr<CommitBatch> commit_batch_;
  base::WeakPtrFactory<LevelDBWrapperImpl> weak_ptr_factory_;

  static bool s_aggressive_flushing_enabled_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBWrapperImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LEVELDB_WRAPPER_IMPL_H_
