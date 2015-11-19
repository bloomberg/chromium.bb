// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_tab_model.h"

#include <stdint.h>

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/android/data_usage/external_data_use_observer.h"
#include "chrome/browser/android/data_usage/tab_data_use_entry.h"
#include "net/base/network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Tracking labels for tests.
const std::string kTestLabel1 = "label_1";
const std::string kTestLabel2 = "label_2";
const std::string kTestLabel3 = "label_3";

const int kTabID1 = 1;
const int kTabID2 = 2;
const int kTabID3 = 3;

enum TabEntrySize { ZERO = 0, ONE, TWO, THREE };

const int kMaxMockObservers = 5;

}  // namespace

namespace base {
class SingleThreadTaskRunner;
}

namespace chrome {

namespace android {

// Test version of |DataUseTabModel|, which permits overriding of calls to Now.
class DataUseTabModelNowTest : public DataUseTabModel {
 public:
  DataUseTabModelNowTest(
      const ExternalDataUseObserver* data_use_observer,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner)
      : DataUseTabModel(data_use_observer, ui_task_runner) {}

  ~DataUseTabModelNowTest() override {}

  void AdvanceTime(base::TimeDelta now_offset) { now_offset_ = now_offset; }

 private:
  // Returns the current time advanced by |now_offset_|.
  base::TimeTicks Now() const override {
    return base::TimeTicks::Now() + now_offset_;
  }

  // Represents the delta offset to be added to current time that is returned by
  // Now.
  base::TimeDelta now_offset_;
};

class DataUseTabModelTest : public testing::Test {
 public:
  DataUseTabModelTest() {}

 protected:
  void SetUp() override {
    // TODO(rajendrant): Create a mock class for ExternalDataUseObserver to
    // spoof the Matches call and test the OnNavigationEvent.
    data_use_tab_model_.reset(
        new DataUseTabModelNowTest(nullptr, task_runner_.get()));
  }

  base::SingleThreadTaskRunner* task_runner() { return task_runner_.get(); }

  // Returns true if tab entry for |tab_id| exists in |active_tabs_|.
  bool IsTabEntryExists(int32_t tab_id) const {
    return data_use_tab_model_->active_tabs_.find(tab_id) !=
           data_use_tab_model_->active_tabs_.end();
  }

  // Checks if there are |expected_size| tab entries being tracked in
  // |active_tabs_|.
  void ExpectTabEntrySize(uint32_t expected_size) const {
    EXPECT_EQ(expected_size, data_use_tab_model_->active_tabs_.size());
  }

  // Returns true if the tracking session for tab with id |tab_id| is currently
  // active.
  bool IsTrackingDataUse(int32_t tab_id) const {
    auto tab_entry_iterator = data_use_tab_model_->active_tabs_.find(tab_id);
    if (tab_entry_iterator == data_use_tab_model_->active_tabs_.end())
      return false;
    return tab_entry_iterator->second.IsTrackingDataUse();
  }

  // Checks if the DataUse object for the given |tab_id| with request start time
  // |at_time| is labeled as an empty string.
  void ExpectEmptyDataUseLabelAtTime(int32_t tab_id,
                                     const base::TimeTicks& at_time) const {
    ExpectDataUseLabelAtTimeWithReturn(tab_id, at_time, false, std::string());
  }

  // Checks if the DataUse object for the given |tab_id| is labeled as an empty
  // string.
  void ExpectEmptyDataUseLabel(int32_t tab_id) const {
    ExpectDataUseLabelAtTimeWithReturn(tab_id, base::TimeTicks::Now(), false,
                                       std::string());
  }

  // Checks if the DataUse object for given |tab_id| is labeled as
  // |expected_label|.
  void ExpectDataUseLabel(int32_t tab_id,
                          const std::string& expected_label) const {
    ExpectDataUseLabelAtTimeWithReturn(tab_id, base::TimeTicks::Now(), true,
                                       expected_label);
  }

  // Checks if GetLabelForDataUse labels the DataUse object for |tab_id| with
  // request start time |at_time|, as |expected_label| and returns
  // |expected_return|.
  void ExpectDataUseLabelAtTimeWithReturn(
      int32_t tab_id,
      const base::TimeTicks& at_time,
      bool expected_return,
      const std::string& expected_label) const {
    data_usage::DataUse data_use(GURL("http://foo.com"), at_time,
                                 GURL("http://foobar.com"), tab_id,
                                 net::NetworkChangeNotifier::CONNECTION_UNKNOWN,
                                 std::string(), 1000, 1000);
    std::string actual_label;
    bool actual_return =
        data_use_tab_model_->GetLabelForDataUse(data_use, &actual_label);
    EXPECT_EQ(expected_return, actual_return);
    EXPECT_EQ(expected_label, actual_label);
  }

  void StartTrackingDataUse(int32_t tab_id, const std::string& label) {
    data_use_tab_model_->StartTrackingDataUse(tab_id, label);
  }

  void EndTrackingDataUse(int32_t tab_id) {
    data_use_tab_model_->EndTrackingDataUse(tab_id);
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_ptr<DataUseTabModelNowTest> data_use_tab_model_;
  base::MessageLoop message_loop_;
};

// Mock observer to track the calls to start and end tracking events.
class MockTabDataUseObserver : public DataUseTabModel::TabDataUseObserver {
 public:
  MOCK_METHOD1(NotifyTrackingStarting, void(int32_t tab_id));
  MOCK_METHOD1(NotifyTrackingEnding, void(int32_t tab_id));
};

// Starts and ends tracking a single tab and checks if its label is returned
// correctly for DataUse objects.
TEST_F(DataUseTabModelTest, SingleTabTracking) {
  ExpectTabEntrySize(TabEntrySize::ZERO);

  // No label is applied initially.
  ExpectEmptyDataUseLabel(kTabID1);
  ExpectEmptyDataUseLabel(kTabID2);

  StartTrackingDataUse(kTabID1, kTestLabel1);
  ExpectTabEntrySize(TabEntrySize::ONE);

  EXPECT_TRUE(IsTrackingDataUse(kTabID1));
  ExpectDataUseLabel(kTabID1, kTestLabel1);
  ExpectEmptyDataUseLabel(kTabID2);

  EndTrackingDataUse(kTabID1);
  ExpectTabEntrySize(TabEntrySize::ONE);
  EXPECT_FALSE(IsTrackingDataUse(kTabID1));
}

// Starts and ends tracking multiple tabs and checks if labels are returned
// correctly for DataUse objects corresponding to different tab ids.
TEST_F(DataUseTabModelTest, MultipleTabTracking) {
  ExpectTabEntrySize(TabEntrySize::ZERO);
  ExpectEmptyDataUseLabel(kTabID1);
  ExpectEmptyDataUseLabel(kTabID2);
  ExpectEmptyDataUseLabel(kTabID3);

  StartTrackingDataUse(kTabID1, kTestLabel1);
  StartTrackingDataUse(kTabID2, kTestLabel2);
  StartTrackingDataUse(kTabID3, kTestLabel3);
  ExpectTabEntrySize(TabEntrySize::THREE);

  EXPECT_TRUE(IsTrackingDataUse(kTabID1));
  EXPECT_TRUE(IsTrackingDataUse(kTabID2));
  EXPECT_TRUE(IsTrackingDataUse(kTabID3));
  ExpectDataUseLabel(kTabID1, kTestLabel1);
  ExpectDataUseLabel(kTabID2, kTestLabel2);
  ExpectDataUseLabel(kTabID3, kTestLabel3);

  EndTrackingDataUse(kTabID1);
  EndTrackingDataUse(kTabID2);
  EndTrackingDataUse(kTabID3);
  ExpectTabEntrySize(TabEntrySize::THREE);
  EXPECT_FALSE(IsTrackingDataUse(kTabID1));
  EXPECT_FALSE(IsTrackingDataUse(kTabID2));
  EXPECT_FALSE(IsTrackingDataUse(kTabID3));

  // Future data use object should be labeled as an empty string.
  base::TimeTicks future_time =
      base::TimeTicks::Now() + base::TimeDelta::FromMilliseconds(20);
  ExpectEmptyDataUseLabelAtTime(kTabID1, future_time);
  ExpectEmptyDataUseLabelAtTime(kTabID2, future_time);
  ExpectEmptyDataUseLabelAtTime(kTabID3, future_time);
}

// Checks that the mock observer receives start and end tracking events for a
// single tab.
TEST_F(DataUseTabModelTest, ObserverStartEndEvents) {
  MockTabDataUseObserver mock_observer;

  EXPECT_CALL(mock_observer, NotifyTrackingStarting(kTabID1)).Times(1);
  EXPECT_CALL(mock_observer, NotifyTrackingEnding(kTabID1)).Times(1);

  data_use_tab_model_->AddObserver(&mock_observer);
  StartTrackingDataUse(kTabID1, kTestLabel1);
  EndTrackingDataUse(kTabID1);

  message_loop_.RunUntilIdle();
}

// Checks that multiple mock observers receive start and end tracking events for
// multiple tabs.
TEST_F(DataUseTabModelTest, MultipleObserverMultipleStartEndEvents) {
  MockTabDataUseObserver mock_observers[kMaxMockObservers];

  for (auto& mock_observer : mock_observers) {
    // Add the observer.
    data_use_tab_model_->AddObserver(&mock_observer);

    // Expect start and end events for tab ids 1-3.
    EXPECT_CALL(mock_observer, NotifyTrackingStarting(kTabID1)).Times(1);
    EXPECT_CALL(mock_observer, NotifyTrackingEnding(kTabID1)).Times(1);
    EXPECT_CALL(mock_observer, NotifyTrackingStarting(kTabID2)).Times(1);
    EXPECT_CALL(mock_observer, NotifyTrackingEnding(kTabID2)).Times(1);
    EXPECT_CALL(mock_observer, NotifyTrackingStarting(kTabID3)).Times(1);
    EXPECT_CALL(mock_observer, NotifyTrackingEnding(kTabID3)).Times(1);
  }

  // Start and end tracking for tab ids 1-3.
  StartTrackingDataUse(kTabID1, kTestLabel1);
  StartTrackingDataUse(kTabID2, kTestLabel2);
  StartTrackingDataUse(kTabID3, kTestLabel3);
  EndTrackingDataUse(kTabID1);
  EndTrackingDataUse(kTabID2);
  EndTrackingDataUse(kTabID3);

  message_loop_.RunUntilIdle();
}

// Checks that the observer is not notified of start and end events after
// RemoveObserver.
TEST_F(DataUseTabModelTest, ObserverNotNotifiedAfterRemove) {
  MockTabDataUseObserver mock_observer;

  // Observer notified of start and end events.
  EXPECT_CALL(mock_observer, NotifyTrackingStarting(kTabID1)).Times(1);
  EXPECT_CALL(mock_observer, NotifyTrackingEnding(kTabID1)).Times(1);

  data_use_tab_model_->AddObserver(&mock_observer);
  StartTrackingDataUse(kTabID1, kTestLabel1);
  EndTrackingDataUse(kTabID1);

  message_loop_.RunUntilIdle();
  testing::Mock::VerifyAndClear(&mock_observer);

  // Observer should not be notified after RemoveObserver.
  EXPECT_CALL(mock_observer, NotifyTrackingStarting(kTabID1)).Times(0);
  EXPECT_CALL(mock_observer, NotifyTrackingEnding(kTabID1)).Times(0);

  data_use_tab_model_->RemoveObserver(&mock_observer);
  StartTrackingDataUse(kTabID1, kTestLabel1);
  EndTrackingDataUse(kTabID1);

  message_loop_.RunUntilIdle();
}

// Checks that tab close event updates the close time of the tab entry.
TEST_F(DataUseTabModelTest, TabCloseEvent) {
  StartTrackingDataUse(kTabID1, kTestLabel1);
  EndTrackingDataUse(kTabID1);

  ExpectTabEntrySize(TabEntrySize::ONE);
  EXPECT_TRUE(
      data_use_tab_model_->active_tabs_[kTabID1].tab_close_time_.is_null());

  data_use_tab_model_->OnTabCloseEvent(kTabID1);

  ExpectTabEntrySize(TabEntrySize::ONE);
  EXPECT_FALSE(
      data_use_tab_model_->active_tabs_[kTabID1].tab_close_time_.is_null());
}

// Checks that tab close event ends the active tracking session for the tab.
TEST_F(DataUseTabModelTest, TabCloseEventEndsTracking) {
  StartTrackingDataUse(kTabID1, kTestLabel1);
  EXPECT_TRUE(IsTrackingDataUse(kTabID1));

  data_use_tab_model_->OnTabCloseEvent(kTabID1);
  EXPECT_FALSE(IsTrackingDataUse(kTabID1));

  // Future data use object should be labeled as an empty string.
  ExpectEmptyDataUseLabelAtTime(
      kTabID1, base::TimeTicks::Now() + base::TimeDelta::FromMilliseconds(20));
}

// Checks that end tracking for specific labels closes those active sessions.
TEST_F(DataUseTabModelTest, OnTrackingLabelRemoved) {
  MockTabDataUseObserver mock_observer;

  StartTrackingDataUse(kTabID1, kTestLabel1);
  StartTrackingDataUse(kTabID2, kTestLabel2);
  StartTrackingDataUse(kTabID3, kTestLabel3);
  data_use_tab_model_->AddObserver(&mock_observer);
  ExpectTabEntrySize(TabEntrySize::THREE);

  EXPECT_TRUE(IsTrackingDataUse(kTabID1));
  EXPECT_TRUE(IsTrackingDataUse(kTabID2));
  EXPECT_TRUE(IsTrackingDataUse(kTabID3));

  // Observer notified of end tracking.
  EXPECT_CALL(mock_observer, NotifyTrackingEnding(kTabID2)).Times(1);

  data_use_tab_model_->OnTrackingLabelRemoved(kTestLabel2);
  message_loop_.RunUntilIdle();

  EXPECT_TRUE(IsTrackingDataUse(kTabID1));
  EXPECT_FALSE(IsTrackingDataUse(kTabID2));
  EXPECT_TRUE(IsTrackingDataUse(kTabID3));

  EXPECT_CALL(mock_observer, NotifyTrackingEnding(kTabID3)).Times(1);

  data_use_tab_model_->OnTrackingLabelRemoved(kTestLabel3);
  message_loop_.RunUntilIdle();

  EXPECT_TRUE(IsTrackingDataUse(kTabID1));
  EXPECT_FALSE(IsTrackingDataUse(kTabID2));
  EXPECT_FALSE(IsTrackingDataUse(kTabID3));
}

// Checks that |active_tabs_| does not grow beyond GetMaxTabEntriesForTests tab
// entries.
TEST_F(DataUseTabModelTest, CompactTabEntriesWithinMaxLimit) {
  const size_t max_tab_entries = data_use_tab_model_->max_tab_entries_;
  uint32_t tab_id = 1;

  ExpectTabEntrySize(TabEntrySize::ZERO);

  while (tab_id <= max_tab_entries) {
    std::string tab_label = base::StringPrintf("label_%d", tab_id);
    StartTrackingDataUse(tab_id, tab_label);
    EndTrackingDataUse(tab_id);

    ExpectTabEntrySize(tab_id);
    ++tab_id;
  }

  uint32_t oldest_tab_id = 1;  // oldest tab entry that will be removed first.

  // Starting and ending more tracking tab entries does not increase the size of
  // |active_tabs_|.
  while (tab_id < max_tab_entries + 10) {
    EXPECT_TRUE(IsTabEntryExists(oldest_tab_id));
    std::string tab_label = base::StringPrintf("label_%d", tab_id);
    StartTrackingDataUse(tab_id, tab_label);
    EndTrackingDataUse(tab_id);

    // Oldest entry got removed.
    EXPECT_FALSE(IsTabEntryExists(oldest_tab_id));
    ExpectTabEntrySize(max_tab_entries);

    ++tab_id;
    ++oldest_tab_id;  // next oldest tab entry.
  }

  // Starting and not ending more tracking tab entries does not increase the
  // size of |active_tabs_|.
  while (tab_id < max_tab_entries + 20) {
    EXPECT_TRUE(IsTabEntryExists(oldest_tab_id));
    std::string tab_label = base::StringPrintf("label_%d", tab_id);
    StartTrackingDataUse(tab_id, tab_label);

    // Oldest entry got removed.
    EXPECT_FALSE(IsTabEntryExists(oldest_tab_id));
    ExpectTabEntrySize(max_tab_entries);

    ++tab_id;
    ++oldest_tab_id;  // next oldest tab entry.
  }
}

TEST_F(DataUseTabModelTest, ExpiredInactiveTabEntryRemovaltimeHistogram) {
  const char kUMAExpiredInactiveTabEntryRemovalDurationSecondsHistogram[] =
      "DataUse.TabModel.ExpiredInactiveTabEntryRemovalDuration";
  base::HistogramTester histogram_tester;

  StartTrackingDataUse(kTabID1, kTestLabel1);
  EndTrackingDataUse(kTabID1);
  EXPECT_FALSE(IsTrackingDataUse(kTabID1));
  data_use_tab_model_->OnTabCloseEvent(kTabID1);

  // Fake tab close time to make it as expired.
  EXPECT_TRUE(IsTabEntryExists(kTabID1));
  auto& tab_entry = data_use_tab_model_->active_tabs_[kTabID1];
  EXPECT_FALSE(tab_entry.tab_close_time_.is_null());
  tab_entry.tab_close_time_ -= tab_entry.closed_tab_expiration_duration_ +
                               base::TimeDelta::FromSeconds(1);
  EXPECT_TRUE(tab_entry.IsExpired());

  // Fast forward 50 seconds.
  data_use_tab_model_->AdvanceTime(base::TimeDelta::FromSeconds(50));

  data_use_tab_model_->CompactTabEntries();
  EXPECT_FALSE(IsTabEntryExists(kTabID1));

  histogram_tester.ExpectTotalCount(
      kUMAExpiredInactiveTabEntryRemovalDurationSecondsHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kUMAExpiredInactiveTabEntryRemovalDurationSecondsHistogram,
      base::TimeDelta::FromSeconds(50).InMilliseconds(), 1);
}

TEST_F(DataUseTabModelTest, UnexpiredTabEntryRemovaltimeHistogram) {
  const char kUMAUnexpiredTabEntryRemovalDurationMinutesHistogram[] =
      "DataUse.TabModel.UnexpiredTabEntryRemovalDuration";
  base::HistogramTester histogram_tester;
  const size_t max_tab_entries = data_use_tab_model_->max_tab_entries_;
  uint32_t tab_id = 1;

  while (tab_id <= max_tab_entries) {
    std::string tab_label = base::StringPrintf("label_%d", tab_id);
    StartTrackingDataUse(tab_id, tab_label);
    EndTrackingDataUse(tab_id);
    ++tab_id;
  }

  // Fast forward 10 minutes.
  data_use_tab_model_->AdvanceTime(base::TimeDelta::FromMinutes(10));

  // Adding another tab entry triggers CompactTabEntries.
  std::string tab_label = base::StringPrintf("label_%d", tab_id);
  StartTrackingDataUse(tab_id, tab_label);
  EndTrackingDataUse(tab_id);

  histogram_tester.ExpectTotalCount(
      kUMAUnexpiredTabEntryRemovalDurationMinutesHistogram, 1);
  histogram_tester.ExpectBucketCount(
      kUMAUnexpiredTabEntryRemovalDurationMinutesHistogram,
      base::TimeDelta::FromMinutes(10).InMilliseconds(), 1);
}

}  // namespace android

}  // namespace chrome
