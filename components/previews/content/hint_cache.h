// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CONTENT_HINT_CACHE_H_
#define COMPONENTS_PREVIEWS_CONTENT_HINT_CACHE_H_

#include <string>

#include "base/callback.h"
#include "base/containers/mru_cache.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/previews/content/hint_cache_store.h"

namespace previews {

using HintLoadedCallback =
    base::OnceCallback<void(const optimization_guide::proto::Hint*)>;

// Contains a set of optimization hints received from the Cacao service. This
// may include hints received from the ComponentUpdater and hints fetched from a
// Cacao Optimization Guide Service API. The availability of hints is queryable
// via host name. The cache itself consists of a backing store, which allows for
// asynchronous loading of any available hint, and an MRU memory cache, which
// can be used to synchronously retrieve recently loaded hints.
class HintCache {
 public:
  // Construct the HintCache with a backing store and an optional max memory
  // cache size. While |hint_store| is required, |max_memory_cache_hints| is
  // optional and the default max size will be used if it is not provided.
  explicit HintCache(
      std::unique_ptr<HintCacheStore> hint_store,
      base::Optional<int> max_memory_cache_hints = base::Optional<int>());
  ~HintCache();

  // Initializes the backing store contained within the hint cache and
  // asynchronously runs the callback after initialization is complete.
  // If |purge_existing_data| is set to true, then the cache will purge any
  // pre-existing data and begin in a clean state.
  void Initialize(bool purge_existing_data, base::OnceClosure callback);

  // Returns a ComponentUpdateData created by the store, based upon the provided
  // component version. During component processing, hints from the component
  // are moved into the component update data. After component processing
  // completes, the component update data is provided to the backing store in
  // UpdateComponentData() and used to update its component hints. In the case
  // the provided component version is not newer than the store's version,
  // nullptr will be returned by the call.
  std::unique_ptr<HintCacheStore::ComponentUpdateData>
  MaybeCreateComponentUpdateData(const base::Version& version) const;

  // Updates the store's component data using the provided ComponentUpdateData
  // and asynchronously runs the provided callback after the update finishes.
  void UpdateComponentData(
      std::unique_ptr<HintCacheStore::ComponentUpdateData> component_data,
      base::OnceClosure callback);

  // Returns whether the cache has a hint data for |host| locally (whether
  // in memory or persisted on disk).
  bool HasHint(const std::string& host) const;

  // Requests that hint data for |host| be loaded asynchronously and passed to
  // |callback| if/when loaded.
  void LoadHint(const std::string& host, HintLoadedCallback callback);

  // Returns the hint data for |host| if found in memory, otherwise nullptr.
  const optimization_guide::proto::Hint* GetHintIfLoaded(
      const std::string& host);

 private:
  using StoreHintMemoryCache =
      base::HashingMRUCache<HintCacheStore::EntryKey,
                            std::unique_ptr<optimization_guide::proto::Hint>>;

  // The callback run after the store finishes initialization. This then runs
  // the callback initially provided by the Initialize() call.
  void OnStoreInitialized(base::OnceClosure callback);

  // The callback run after the store finishes loading a hint. This adds the
  // loaded hint to |memory_cache_|, potentially purging the least recently
  // used element, and then runs the callback initially provided by the
  // LoadHint() call.
  void OnLoadStoreHint(HintLoadedCallback callback,
                       const HintCacheStore::EntryKey& store_hint_entry_key,
                       std::unique_ptr<optimization_guide::proto::Hint> hint);

  // Finds the most specific host suffix of the host name for which the store
  // has a hint and populates |out_hint_entry_key| with the hint's corresponding
  // entry key. Returns true if a hint was successfully found.
  bool FindHintEntryKey(const std::string& host,
                        HintCacheStore::EntryKey* out_hint_entry_key) const;

  // The backing store used with this hint cache. Set during construction.
  const std::unique_ptr<HintCacheStore> hint_store_;

  // The in-memory cache of hints loaded from the store. Maps store EntryKey to
  // Hint proto. This servers two purposes:
  //  1. Allows hints to be requested on navigation and retained in memory until
  //     commit, when they can be synchronously retrieved from the cache.
  //  2. Reduces churn of needing to reload hints from frequently visited sites
  //     multiple times during a session.
  StoreHintMemoryCache memory_cache_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(HintCache);
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CONTENT_HINT_CACHE_H_
