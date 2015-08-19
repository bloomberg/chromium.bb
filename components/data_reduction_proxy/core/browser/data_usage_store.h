// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_USAGE_STORE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_USAGE_STORE_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/sequence_checker.h"

namespace base {

class Time;

}  // namespace base

namespace data_reduction_proxy {
class DataStore;
class DataUsageBucket;

// Store for detailed data usage stats. Data usage from every
// |kDataUsageBucketLengthMins| interval is stored in a DataUsageBucket.
class DataUsageStore {
 public:
  explicit DataUsageStore(DataStore* db);

  ~DataUsageStore();

  // Loads the data usage bucket for the current interval into |current_bucket|.
  // This method must be called at least once before any calls to
  // |StoreCurrentDataUsageBucket|.
  void LoadCurrentDataUsageBucket(DataUsageBucket* bucket);

  // Stores the data usage bucket for the current interval. This will overwrite
  // the current data usage bucket in the |db_| if they are for the same
  // interval. It will also backfill any missed intervals with empty data.
  // Intervals might be missed because Chrome was not running, or there was no
  // network activity during an interval.
  void StoreCurrentDataUsageBucket(const DataUsageBucket& current_bucket);

  // Returns whether |time| is within the current interval. Each hour is
  // divided into |kDataUsageBucketLengthMins| minute long intervals.
  static bool IsInCurrentInterval(const base::Time& time);

 private:
  friend class DataUsageStoreTest;

  // Converts the given |bucket| into a string format for persistance to
  // |DataReductionProxyStore| and add to the map.
  void AddBucketToMap(const DataUsageBucket& bucket,
                      std::map<std::string, std::string>* map);

  // Returns the number of buckets between the current interval bucket and the
  // last bucket that was persisted to the store.
  int NumBucketsSinceLastSaved(base::Time current) const;

  // The store to persist data usage information.
  DataStore* db_;

  // The index of the last bucket persisted in the |db_|. |DataUsageBucket| is
  // stored in the |db_| as a circular array. This index points to the array
  // position corresponding to the current bucket.
  int current_bucket_index_;

  // The time when the current bucket was last written to |db_|. This field is
  // used to determine if a DataUsageBucket to be saved belongs to the same
  // interval, or a more recent interval.
  base::Time current_bucket_last_updated_;

  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(DataUsageStore);
};

}  // namespace data_reduction_proxy
#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_USAGE_STORE_H_
