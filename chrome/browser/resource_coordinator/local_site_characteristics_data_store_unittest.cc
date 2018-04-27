// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store.h"

#include "base/macros.h"
#include "base/test/simple_test_tick_clock.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_impl.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

namespace {
const char kTestOrigin[] = "http://www.foo.com";
const char kTestOrigin2[] = "http://www.bar.com";
}  // namespace

class LocalSiteCharacteristicsDataStoreTest : public ::testing::Test {
 public:
  LocalSiteCharacteristicsDataStoreTest()
      : scoped_set_tick_clock_for_testing_(&test_clock_),
        data_store_(&profile_) {
    test_clock_.SetNowTicks(base::TimeTicks::UnixEpoch());
    test_clock_.Advance(base::TimeDelta::FromHours(1));
  }

 protected:
  base::SimpleTestTickClock test_clock_;
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_;
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  TestingProfile profile_;
  LocalSiteCharacteristicsDataStore data_store_;
};

TEST_F(LocalSiteCharacteristicsDataStoreTest, EndToEnd) {
  auto reader = data_store_.GetReaderForOrigin(kTestOrigin);
  EXPECT_NE(nullptr, reader.get());
  EXPECT_EQ(1U, data_store_.origin_data_map_for_testing().size());

  internal::LocalSiteCharacteristicsDataImpl* data =
      data_store_.origin_data_map_for_testing().find(kTestOrigin)->second;
  EXPECT_NE(nullptr, data);

  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());
  data->NotifySiteLoaded();
  data->NotifyUpdatesTitleInBackground();
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            reader->UpdatesTitleInBackground());
  data->NotifySiteUnloaded();
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            reader->UpdatesTitleInBackground());

  auto reader_copy = data_store_.GetReaderForOrigin(kTestOrigin);
  EXPECT_EQ(1U, data_store_.origin_data_map_for_testing().size());
  auto reader2 = data_store_.GetReaderForOrigin(kTestOrigin2);
  EXPECT_EQ(2U, data_store_.origin_data_map_for_testing().size());
  reader2.reset();
  EXPECT_EQ(1U, data_store_.origin_data_map_for_testing().size());
  reader_copy.reset();

  reader.reset();
  EXPECT_TRUE(data_store_.origin_data_map_for_testing().empty());

  data_store_.OnURLsDeleted(nullptr, history::DeletionTimeRange::AllTime(),
                            false, history::URLRows(), std::set<GURL>());
}

TEST_F(LocalSiteCharacteristicsDataStoreTest, HistoryServiceObserver) {
  // Load a first origin, and then make use of a feature on it.

  const std::string kOrigin1Url = GURL(kTestOrigin).GetOrigin().GetContent();
  auto reader = data_store_.GetReaderForOrigin(kOrigin1Url);
  EXPECT_TRUE(reader);
  internal::LocalSiteCharacteristicsDataImpl* data =
      data_store_.origin_data_map_for_testing().find(kOrigin1Url)->second;
  EXPECT_NE(nullptr, data);

  constexpr base::TimeDelta kDelay = base::TimeDelta::FromHours(1);

  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());
  data->NotifySiteLoaded();
  base::TimeDelta last_loaded_time = data->last_loaded_time_for_testing();
  data->NotifyUpdatesTitleInBackground();
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            reader->UpdatesTitleInBackground());
  test_clock_.Advance(kDelay);

  // Load a second origin, make use of a feature on it too.
  const std::string kOrigin2Url = GURL(kTestOrigin2).GetOrigin().GetContent();
  auto reader2 = data_store_.GetReaderForOrigin(kOrigin2Url);
  EXPECT_TRUE(reader2);
  internal::LocalSiteCharacteristicsDataImpl* data2 =
      data_store_.origin_data_map_for_testing().find(kOrigin2Url)->second;
  EXPECT_NE(nullptr, data2);
  data2->NotifySiteLoaded();
  data2->NotifyUpdatesFaviconInBackground();

  // This site hasn'be been unloaded yet, so the last loaded time shouldn't have
  // changed.
  EXPECT_EQ(data->last_loaded_time_for_testing(), last_loaded_time);

  history::URLRows urls_to_delete = {
      history::URLRow(GURL(kTestOrigin)),
      history::URLRow(GURL("http://www.url-not-in-map.com"))};
  data_store_.OnURLsDeleted(nullptr, history::DeletionTimeRange::Invalid(),
                            false, urls_to_delete, std::set<GURL>());

  // The information for this site have been reset, so the last loaded time
  // should now be equal to the current time and the title update feature
  // observation should have been cleared.
  EXPECT_NE(data->last_loaded_time_for_testing(), last_loaded_time);
  EXPECT_EQ(data->last_loaded_time_for_testing(),
            test_clock_.NowTicks() - base::TimeTicks::UnixEpoch());
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());
  // The second site shouldn't have been cleared.
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            reader2->UpdatesFaviconInBackground());

  test_clock_.Advance(kDelay);

  // Delete all the information stored in the data store.
  data_store_.OnURLsDeleted(nullptr, history::DeletionTimeRange::AllTime(),
                            false, history::URLRows(), std::set<GURL>());

  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader2->UpdatesFaviconInBackground());
  EXPECT_EQ(data->last_loaded_time_for_testing(),
            test_clock_.NowTicks() - base::TimeTicks::UnixEpoch());
  EXPECT_EQ(data2->last_loaded_time_for_testing(),
            test_clock_.NowTicks() - base::TimeTicks::UnixEpoch());

  data->NotifySiteUnloaded();
  data2->NotifySiteUnloaded();
}

}  // namespace resource_coordinator
