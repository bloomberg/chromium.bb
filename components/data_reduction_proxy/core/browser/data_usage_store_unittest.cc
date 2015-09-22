// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_store.h"
#include "components/data_reduction_proxy/core/browser/data_usage_store.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// Each bucket holds data usage for a 5 minute interval. History is maintained
// for 60 days.
const unsigned kNumExpectedBuckets = 60 * 24 * 60 / 5;
}

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

  std::vector<DataUsageBucket> data_usage;
  data_usage_store()->LoadDataUsage(&data_usage);

  ASSERT_EQ(kNumExpectedBuckets, data_usage.size());
  ASSERT_FALSE(data_usage[0].has_last_updated_timestamp());
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

  std::vector<DataUsageBucket> data_usage;
  data_usage_store()->LoadDataUsage(&data_usage);

  ASSERT_EQ(kNumExpectedBuckets, data_usage.size());
  ASSERT_FALSE(data_usage[0].has_last_updated_timestamp());
  ASSERT_FALSE(
      data_usage[kNumExpectedBuckets - 2].has_last_updated_timestamp());
  ASSERT_EQ(now.ToInternalValue(),
            data_usage[kNumExpectedBuckets - 1].last_updated_timestamp());
}

TEST_F(DataUsageStoreTest, StoreSameBucket) {
  base::Time::Exploded exploded;
  exploded.year = 2001;
  exploded.month = 1;
  exploded.day_of_month = 1;
  exploded.hour = 0;

  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;
  base::Time time1 = base::Time::FromUTCExploded(exploded);

  exploded.minute = 4;
  exploded.second = 59;
  exploded.millisecond = 999;
  base::Time time2 = base::Time::FromUTCExploded(exploded);

  DataUsageBucket bucket;
  data_usage_store()->LoadCurrentDataUsageBucket(&bucket);

  bucket.set_last_updated_timestamp(time1.ToInternalValue());

  data_usage_store()->StoreCurrentDataUsageBucket(bucket);
  ASSERT_EQ(2u, store()->map()->size());

  bucket.set_last_updated_timestamp(time2.ToInternalValue());
  data_usage_store()->StoreCurrentDataUsageBucket(bucket);
  ASSERT_EQ(2u, store()->map()->size());

  std::vector<DataUsageBucket> data_usage;
  data_usage_store()->LoadDataUsage(&data_usage);

  ASSERT_EQ(kNumExpectedBuckets, data_usage.size());
  ASSERT_FALSE(data_usage[0].has_last_updated_timestamp());
  ASSERT_FALSE(
      data_usage[kNumExpectedBuckets - 2].has_last_updated_timestamp());
  ASSERT_EQ(time2.ToInternalValue(),
            data_usage[kNumExpectedBuckets - 1].last_updated_timestamp());
}

TEST_F(DataUsageStoreTest, StoreConsecutiveBuckets) {
  base::Time::Exploded exploded;
  exploded.year = 2001;
  exploded.month = 1;
  exploded.day_of_month = 1;
  exploded.hour = 0;

  exploded.minute = 0;
  exploded.second = 59;
  exploded.millisecond = 999;
  base::Time time1 = base::Time::FromUTCExploded(exploded);

  exploded.minute = 5;
  exploded.second = 0;
  exploded.millisecond = 0;
  base::Time time2 = base::Time::FromUTCExploded(exploded);

  DataUsageBucket bucket;
  data_usage_store()->LoadCurrentDataUsageBucket(&bucket);

  bucket.set_last_updated_timestamp(time1.ToInternalValue());

  data_usage_store()->StoreCurrentDataUsageBucket(bucket);
  ASSERT_EQ(2u, store()->map()->size());

  bucket.set_last_updated_timestamp(time2.ToInternalValue());
  data_usage_store()->StoreCurrentDataUsageBucket(bucket);
  ASSERT_EQ(3u, store()->map()->size());

  std::vector<DataUsageBucket> data_usage;
  data_usage_store()->LoadDataUsage(&data_usage);

  ASSERT_EQ(kNumExpectedBuckets, data_usage.size());
  ASSERT_FALSE(data_usage[0].has_last_updated_timestamp());
  ASSERT_FALSE(
      data_usage[kNumExpectedBuckets - 3].has_last_updated_timestamp());
  ASSERT_EQ(time1.ToInternalValue(),
            data_usage[kNumExpectedBuckets - 2].last_updated_timestamp());
  ASSERT_EQ(time2.ToInternalValue(),
            data_usage[kNumExpectedBuckets - 1].last_updated_timestamp());
}

TEST_F(DataUsageStoreTest, StoreMultipleBuckets) {
  DataUsageBucket bucket;
  data_usage_store()->LoadCurrentDataUsageBucket(&bucket);

  // Comments indicate time expressed as day.hour.min.sec.millis relative to
  // each other beginning at 0.0.0.0.0.
  // The first bucket range is 0.0.0.0.0 - 0.0.4.59.999 and
  // the second bucket range is 0.0.5.0.0 - 0.0.9.59.999, etc.
  base::Time first_bucket_time = base::Time::Now();  // 0.0.0.0.0.
  base::Time last_bucket_time = first_bucket_time    // 59.23.55.0.0
                                + base::TimeDelta::FromDays(60) -
                                base::TimeDelta::FromMinutes(5);
  base::Time before_history_time =  // 0.0.-5.0.0
      first_bucket_time - base::TimeDelta::FromMinutes(5);
  base::Time tenth_bucket_time =  // 0.0.45.0.0
      first_bucket_time + base::TimeDelta::FromMinutes(45);
  base::Time second_last_bucket_time =  // 59.23.50.0.0
      last_bucket_time - base::TimeDelta::FromMinutes(5);

  // This bucket will be discarded when the |last_bucket| is stored.
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
  // 9 more (empty) buckets should have been added to the store.
  ASSERT_EQ(12u, store()->map()->size());

  // This will be the last but one bucket when |last_bucket| is stored.
  DataUsageBucket second_last_bucket;
  second_last_bucket.set_last_updated_timestamp(
      second_last_bucket_time.ToInternalValue());
  data_usage_store()->StoreCurrentDataUsageBucket(second_last_bucket);
  // Max number of buckets we store to DB plus one for the current index.
  ASSERT_EQ(kNumExpectedBuckets + 1, store()->map()->size());

  // This bucket should simply overwrite oldest bucket, so number of entries in
  // store should be unchanged.
  DataUsageBucket last_bucket;
  last_bucket.set_last_updated_timestamp(last_bucket_time.ToInternalValue());
  data_usage_store()->StoreCurrentDataUsageBucket(last_bucket);
  ASSERT_EQ(kNumExpectedBuckets + 1, store()->map()->size());

  std::vector<DataUsageBucket> data_usage;
  data_usage_store()->LoadDataUsage(&data_usage);

  ASSERT_EQ(kNumExpectedBuckets, data_usage.size());
  ASSERT_EQ(first_bucket_time.ToInternalValue(),
            data_usage[0].last_updated_timestamp());
  ASSERT_EQ(tenth_bucket_time.ToInternalValue(),
            data_usage[9].last_updated_timestamp());
  ASSERT_EQ(second_last_bucket_time.ToInternalValue(),
            data_usage[kNumExpectedBuckets - 2].last_updated_timestamp());
  ASSERT_EQ(last_bucket_time.ToInternalValue(),
            data_usage[kNumExpectedBuckets - 1].last_updated_timestamp());
}

TEST_F(DataUsageStoreTest, DeleteHistoricalDataUsage) {
  DataUsageBucket bucket;
  data_usage_store()->LoadCurrentDataUsageBucket(&bucket);

  bucket.set_last_updated_timestamp(base::Time::Now().ToInternalValue());

  data_usage_store()->StoreCurrentDataUsageBucket(bucket);
  ASSERT_EQ(2u, store()->map()->size());

  data_usage_store()->DeleteHistoricalDataUsage();
  ASSERT_EQ(0u, store()->map()->size());
}

}  // namespace data_reduction_proxy
