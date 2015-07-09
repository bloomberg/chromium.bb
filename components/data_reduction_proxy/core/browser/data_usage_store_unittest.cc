// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/browser/data_usage_store.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// In memory hash map implemention of |DataStore| for testing.
class TestDataStore : public data_reduction_proxy::DataStore {
 public:
  ~TestDataStore() override {}

  void InitializeOnDBThread() override {}

  Status Get(const std::string& key, std::string* value) override {
    auto value_iter = map_.find(key);
    if (value_iter == map_.end()) {
      return NOT_FOUND;
    }

    value->assign(value_iter->second);
    return OK;
  }

  Status Put(const std::map<std::string, std::string>& map) override {
    auto iter = map.begin();
    map_.insert(iter, map.end());

    return OK;
  }

  std::map<std::string, std::string>* map() { return &map_; }

 private:
  std::map<std::string, std::string> map_;
};

}  // namespace

namespace data_reduction_proxy {

class DataUsageStoreTest : public testing::Test {
 protected:
  void SetUp() override {
    store_.reset(new TestDataStore());
    data_usage_store_.reset(new DataUsageStore(store_.get()));
  }

  int current_bucket_index() const {
    return data_usage_store_->current_bucket_index_;
  }

  int64 current_bucket_last_updated() const {
    return data_usage_store_->current_bucket_last_updated_.ToInternalValue();
  }

  TestDataStore* store() const { return store_.get(); }

  DataUsageStore* data_usage_store() const { return data_usage_store_.get(); }

 private:
  scoped_ptr<TestDataStore> store_;
  scoped_ptr<DataUsageStore> data_usage_store_;
};

TEST_F(DataUsageStoreTest, LoadEmpty) {
  ASSERT_NE(0, current_bucket_index());

  DataUsageBucket bucket;
  data_usage_store()->LoadCurrentDataUsageBucket(&bucket);

  ASSERT_EQ(0, current_bucket_index());
  ASSERT_EQ(0, current_bucket_last_updated());
  ASSERT_FALSE(bucket.has_last_updated_timestamp());
}

TEST_F(DataUsageStoreTest, LoadAndStoreToSameBucket) {
  DataUsageBucket bucket;
  data_usage_store()->LoadCurrentDataUsageBucket(&bucket);

  base::Time now = base::Time::Now();
  bucket.set_last_updated_timestamp(now.ToInternalValue());

  data_usage_store()->StoreCurrentDataUsageBucket(bucket);
  ASSERT_EQ(2u, store()->map()->size());

  DataUsageBucket stored_bucket;
  data_usage_store()->LoadCurrentDataUsageBucket(&stored_bucket);
  ASSERT_EQ(now.ToInternalValue(), stored_bucket.last_updated_timestamp());
}

TEST_F(DataUsageStoreTest, StoreWithEmptyBuckets) {
  DataUsageBucket bucket;
  data_usage_store()->LoadCurrentDataUsageBucket(&bucket);

  base::Time last_bucket_time = base::Time::Now();
  base::Time first_bucket_time =
      last_bucket_time - base::TimeDelta::FromDays(60);
  base::Time before_history_time =
      first_bucket_time - base::TimeDelta::FromMinutes(15);
  base::Time tenth_bucket_time =
      first_bucket_time + base::TimeDelta::FromMinutes(15 * 10);
  base::Time second_last_bucket_time =
      last_bucket_time - base::TimeDelta::FromMinutes(15);

  // This bucket will be discarded when the |last_bucket| below in stored.
  DataUsageBucket bucket_before_history;
  bucket_before_history.set_last_updated_timestamp(
      before_history_time.ToInternalValue());
  data_usage_store()->StoreCurrentDataUsageBucket(bucket_before_history);
  // Only one bucket has been stored, so store should have 2 entries, one for
  // current index and one for the bucket itself.
  ASSERT_EQ(2u, store()->map()->size());

  // Calling |StoreCurrentDataUsageBucket| with the same last updated timestamp
  // should not cause number of stored buckets to increase and current bucket
  // should be overwritten.
  data_usage_store()->StoreCurrentDataUsageBucket(bucket_before_history);
  // Only one bucket has been stored, so store should have 2 entries, one for
  // current index and one for the bucket itself.
  ASSERT_EQ(2u, store()->map()->size());

  // This will be the very first bucket once |last_bucket| is stored.
  DataUsageBucket first_bucket;
  first_bucket.set_last_updated_timestamp(first_bucket_time.ToInternalValue());
  data_usage_store()->StoreCurrentDataUsageBucket(first_bucket);
  // A new bucket has been stored, so entires in map should increase by one.
  ASSERT_EQ(3u, store()->map()->size());

  // This will be the 10th bucket when |last_bucket| is stored. We skipped
  // calling |StoreCurrentDataUsageBucket| on 10 buckets.
  DataUsageBucket tenth_bucket;
  tenth_bucket.set_last_updated_timestamp(tenth_bucket_time.ToInternalValue());
  data_usage_store()->StoreCurrentDataUsageBucket(tenth_bucket);
  // 10 more (empty) buckets should have been added to the store.
  ASSERT_EQ(13u, store()->map()->size());

  // This will be the last but one bucket when |last_bucket| is stored.
  DataUsageBucket second_last_bucket;
  second_last_bucket.set_last_updated_timestamp(
      second_last_bucket_time.ToInternalValue());
  data_usage_store()->StoreCurrentDataUsageBucket(second_last_bucket);
  // Max number of buckets we store to DB plus one for the current index.
  ASSERT_EQ(5761u, store()->map()->size());

  // This bucket should simply overwrite oldest bucket, so number of entries in
  // store should be unchanged.
  DataUsageBucket last_bucket;
  last_bucket.set_last_updated_timestamp(last_bucket_time.ToInternalValue());
  data_usage_store()->StoreCurrentDataUsageBucket(last_bucket);
  ASSERT_EQ(5761u, store()->map()->size());
}

}  // namespace data_reduction_proxy
