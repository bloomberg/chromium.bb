// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/data_use_tab_model.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/android/data_usage/data_use_matcher.h"
#include "chrome/browser/android/data_usage/external_data_use_observer_bridge.h"
#include "chrome/browser/android/data_usage/tab_data_use_entry.h"
#include "components/data_usage/core/data_use_aggregator.h"
#include "components/data_usage/core/data_use_amortizer.h"
#include "components/data_usage/core/data_use_annotator.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// Tracking labels for tests.
const char kTestLabel1[] = "label_1";
const char kTestLabel2[] = "label_2";
const char kTestLabel3[] = "label_3";

const int kTabID1 = 1;
const int kTabID2 = 2;
const int kTabID3 = 3;

const char kURLFoo[] = "http://foo.com/";
const char kURLBar[] = "http://bar.com/";
const char kURLBaz[] = "http://baz.com/";
const char kURLFooBar[] = "http://foobar.com/";
const char kPackageFoo[] = "com.google.package.foo";
const char kPackageBar[] = "com.google.package.bar";

enum TabEntrySize { ZERO = 0, ONE, TWO, THREE };

// Indicates the tracking states for a sequence of navigations.
enum TrackingState {
  NONE,
  STARTED,
  ENDED,
  CONTINUES,
};

// Mock observer to track the calls to start and end tracking events.
class MockTabDataUseObserver
    : public chrome::android::DataUseTabModel::TabDataUseObserver {
 public:
  MOCK_METHOD1(NotifyTrackingStarting, void(SessionID::id_type tab_id));
  MOCK_METHOD1(NotifyTrackingEnding, void(SessionID::id_type tab_id));
  MOCK_METHOD0(OnDataUseTabModelReady, void());
};

class TestExternalDataUseObserverBridge
    : public chrome::android::ExternalDataUseObserverBridge {
 public:
  TestExternalDataUseObserverBridge() {}
  void FetchMatchingRules() const override {}
  void ShouldRegisterAsDataUseObserver(bool should_register) const override{};
};

}  // namespace

namespace chrome {

namespace android {

class DataUseTabModelTest : public testing::Test {
 public:
  DataUseTabModelTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        external_data_use_observer_bridge_(
            new TestExternalDataUseObserverBridge()) {}

 protected:
  void SetUp() override {
    base::RunLoop().RunUntilIdle();
    data_use_tab_model_.reset(new DataUseTabModel());
    data_use_tab_model_->InitOnUIThread(
        external_data_use_observer_bridge_.get());

    tick_clock_ = new base::SimpleTestTickClock();

    // Advance to non nil time.
    tick_clock_->Advance(base::TimeDelta::FromSeconds(1));

    // |tick_clock_| will be owned by |data_use_tab_model_|.
    data_use_tab_model_->tick_clock_.reset(tick_clock_);
    data_use_tab_model_->OnControlAppInstallStateChange(true);
  }

  // Returns true if tab entry for |tab_id| exists in |active_tabs_|.
  bool IsTabEntryExists(SessionID::id_type tab_id) const {
    return data_use_tab_model_->active_tabs_.find(tab_id) !=
           data_use_tab_model_->active_tabs_.end();
  }

  // Checks if there are |expected_size| tab entries being tracked in
  // |active_tabs_|.
  void ExpectTabEntrySize(uint32_t expected_size) const {
    EXPECT_EQ(expected_size, data_use_tab_model_->active_tabs_.size());
  }

  // Returns true if |tab_id| is a custom tab and started tracking due to
  // package match.
  bool IsCustomTabPackageMatch(SessionID::id_type tab_id) const {
    auto tab_entry_iterator = data_use_tab_model_->active_tabs_.find(tab_id);
    return (tab_entry_iterator != data_use_tab_model_->active_tabs_.end()) &&
           tab_entry_iterator->second.is_custom_tab_package_match();
  }

  // Returns true if the tracking session for tab with id |tab_id| is currently
  // active.
  bool IsTrackingDataUse(SessionID::id_type tab_id) const {
    const auto& tab_entry_iterator =
        data_use_tab_model_->active_tabs_.find(tab_id);
    if (tab_entry_iterator == data_use_tab_model_->active_tabs_.end())
      return false;
    return tab_entry_iterator->second.IsTrackingDataUse();
  }

  // Checks if the DataUse object for the given |tab_id| with request start time
  // |at_time| is labeled as an empty string.
  void ExpectEmptyDataUseLabelAtTime(SessionID::id_type tab_id,
                                     const base::TimeTicks& at_time) const {
    ExpectDataUseLabelAtTimeWithReturn(tab_id, at_time, false, std::string());
  }

  // Checks if the DataUse object for the given |tab_id| is labeled as an empty
  // string.
  void ExpectEmptyDataUseLabel(SessionID::id_type tab_id) const {
    ExpectDataUseLabelAtTimeWithReturn(tab_id, tick_clock_->NowTicks(), false,
                                       std::string());
  }

  // Checks if the DataUse object for given |tab_id| is labeled as
  // |expected_label|.
  void ExpectDataUseLabel(SessionID::id_type tab_id,
                          const std::string& expected_label) const {
    ExpectDataUseLabelAtTimeWithReturn(tab_id, tick_clock_->NowTicks(),
                                       !expected_label.empty(), expected_label);
  }

  // Checks if GetLabelForDataUse labels the DataUse object for |tab_id| with
  // request start time |at_time|, as |expected_label| and returns
  // |expected_return|.
  void ExpectDataUseLabelAtTimeWithReturn(
      SessionID::id_type tab_id,
      const base::TimeTicks& at_time,
      bool expected_return,
      const std::string& expected_label) const {
    std::string actual_label;
    bool actual_return = data_use_tab_model_->GetLabelForTabAtTime(
        tab_id, at_time, &actual_label);
    EXPECT_EQ(expected_return, actual_return);
    EXPECT_EQ(expected_label, actual_label);
  }

  void StartTrackingDataUse(SessionID::id_type tab_id,
                            const std::string& label) {
    data_use_tab_model_->StartTrackingDataUse(tab_id, label, false);
  }

  void EndTrackingDataUse(SessionID::id_type tab_id) {
    data_use_tab_model_->EndTrackingDataUse(tab_id);
  }

  void RegisterURLRegexes(const std::vector<std::string>& app_package_names,
                          const std::vector<std::string>& domain_regexes,
                          const std::vector<std::string>& labels) {
    data_use_tab_model_->RegisterURLRegexes(app_package_names, domain_regexes,
                                            labels);
  }

  // Pointer to the tick clock owned by |data_use_tab_model_|.
  base::SimpleTestTickClock* tick_clock_;

  std::unique_ptr<DataUseTabModel> data_use_tab_model_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<ExternalDataUseObserverBridge>
      external_data_use_observer_bridge_;

  DISALLOW_COPY_AND_ASSIGN(DataUseTabModelTest);
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
      tick_clock_->NowTicks() + base::TimeDelta::FromMilliseconds(20);
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
}

// Checks that multiple mock observers receive start and end tracking events for
// multiple tabs.
TEST_F(DataUseTabModelTest, MultipleObserverMultipleStartEndEvents) {
  const int kMaxMockObservers = 5;
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

  testing::Mock::VerifyAndClear(&mock_observer);

  // Observer should not be notified after RemoveObserver.
  EXPECT_CALL(mock_observer, NotifyTrackingStarting(kTabID1)).Times(0);
  EXPECT_CALL(mock_observer, NotifyTrackingEnding(kTabID1)).Times(0);

  data_use_tab_model_->RemoveObserver(&mock_observer);
  StartTrackingDataUse(kTabID1, kTestLabel1);
  EndTrackingDataUse(kTabID1);
}

// Checks that tab close event updates the close time of the tab entry.
TEST_F(DataUseTabModelTest, TabCloseEvent) {
  StartTrackingDataUse(kTabID1, kTestLabel1);
  EndTrackingDataUse(kTabID1);

  ExpectTabEntrySize(TabEntrySize::ONE);
  EXPECT_TRUE(data_use_tab_model_->active_tabs_.find(kTabID1)
                  ->second.tab_close_time_.is_null());

  data_use_tab_model_->OnTabCloseEvent(kTabID1);

  ExpectTabEntrySize(TabEntrySize::ONE);
  EXPECT_FALSE(data_use_tab_model_->active_tabs_.find(kTabID1)
                   ->second.tab_close_time_.is_null());
}

// Checks that tab close event ends the active tracking session for the tab.
TEST_F(DataUseTabModelTest, TabCloseEventEndsTracking) {
  StartTrackingDataUse(kTabID1, kTestLabel1);
  EXPECT_TRUE(IsTrackingDataUse(kTabID1));

  data_use_tab_model_->OnTabCloseEvent(kTabID1);
  EXPECT_FALSE(IsTrackingDataUse(kTabID1));

  // Future data use object should be labeled as an empty string.
  ExpectEmptyDataUseLabelAtTime(
      kTabID1, tick_clock_->NowTicks() + base::TimeDelta::FromMilliseconds(20));
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

  // Observer not notified of end tracking.
  EXPECT_CALL(mock_observer, NotifyTrackingEnding(kTabID2)).Times(0);

  data_use_tab_model_->OnTrackingLabelRemoved(kTestLabel2);

  EXPECT_TRUE(IsTrackingDataUse(kTabID1));
  EXPECT_FALSE(IsTrackingDataUse(kTabID2));
  EXPECT_TRUE(IsTrackingDataUse(kTabID3));

  // Observer not notified of end tracking.
  EXPECT_CALL(mock_observer, NotifyTrackingEnding(kTabID3)).Times(0);

  data_use_tab_model_->OnTrackingLabelRemoved(kTestLabel3);

  EXPECT_TRUE(IsTrackingDataUse(kTabID1));
  EXPECT_FALSE(IsTrackingDataUse(kTabID2));
  EXPECT_FALSE(IsTrackingDataUse(kTabID3));
}

// Checks that |active_tabs_| does not grow beyond GetMaxTabEntriesForTests tab
// entries.
TEST_F(DataUseTabModelTest, CompactTabEntriesWithinMaxLimit) {
  const int32_t max_tab_entries =
      static_cast<int32_t>(data_use_tab_model_->max_tab_entries_);
  SessionID::id_type tab_id = 1;

  ExpectTabEntrySize(TabEntrySize::ZERO);

  while (tab_id <= max_tab_entries) {
    std::string tab_label = base::StringPrintf("label_%d", tab_id);
    StartTrackingDataUse(tab_id, tab_label);
    tick_clock_->Advance(base::TimeDelta::FromSeconds(1));
    EndTrackingDataUse(tab_id);
    tick_clock_->Advance(base::TimeDelta::FromSeconds(1));

    ExpectTabEntrySize(tab_id);
    ++tab_id;
  }

  // Oldest tab entry that will be removed first.
  SessionID::id_type oldest_tab_id = 1;

  // Starting and ending more tracking tab entries does not increase the size of
  // |active_tabs_|.
  while (tab_id < max_tab_entries + 10) {
    EXPECT_TRUE(IsTabEntryExists(oldest_tab_id));
    std::string tab_label = base::StringPrintf("label_%d", tab_id);
    StartTrackingDataUse(tab_id, tab_label);
    tick_clock_->Advance(base::TimeDelta::FromSeconds(1));
    EndTrackingDataUse(tab_id);
    tick_clock_->Advance(base::TimeDelta::FromSeconds(1));

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
    tick_clock_->Advance(base::TimeDelta::FromSeconds(1));

    // Oldest entry got removed.
    EXPECT_FALSE(IsTabEntryExists(oldest_tab_id));
    ExpectTabEntrySize(max_tab_entries);

    ++tab_id;
    ++oldest_tab_id;  // next oldest tab entry.
  }
}

TEST_F(DataUseTabModelTest, ExpiredInactiveTabEntryRemovaltimeHistogram) {
  const char kUMAExpiredInactiveTabEntryRemovalDurationHistogram[] =
      "DataUsage.TabModel.ExpiredInactiveTabEntryRemovalDuration";
  base::HistogramTester histogram_tester;

  StartTrackingDataUse(kTabID1, kTestLabel1);
  EndTrackingDataUse(kTabID1);
  EXPECT_FALSE(IsTrackingDataUse(kTabID1));
  data_use_tab_model_->OnTabCloseEvent(kTabID1);

  // Fake tab close time to make it as expired.
  EXPECT_TRUE(IsTabEntryExists(kTabID1));
  auto& tab_entry = data_use_tab_model_->active_tabs_.find(kTabID1)->second;
  EXPECT_FALSE(tab_entry.tab_close_time_.is_null());
  tab_entry.tab_close_time_ -=
      data_use_tab_model_->closed_tab_expiration_duration() +
      base::TimeDelta::FromSeconds(1);
  EXPECT_TRUE(tab_entry.IsExpired());

  // Fast forward 50 seconds.
  tick_clock_->Advance(base::TimeDelta::FromSeconds(50));

  data_use_tab_model_->CompactTabEntries();
  EXPECT_FALSE(IsTabEntryExists(kTabID1));

  histogram_tester.ExpectUniqueSample(
      kUMAExpiredInactiveTabEntryRemovalDurationHistogram,
      base::TimeDelta::FromSeconds(50).InMilliseconds(), 1);
}

TEST_F(DataUseTabModelTest, UnexpiredTabEntryRemovaltimeHistogram) {
  const char kUMAUnexpiredTabEntryRemovalDurationHistogram[] =
      "DataUsage.TabModel.UnexpiredTabEntryRemovalDuration";
  base::HistogramTester histogram_tester;
  const int32_t max_tab_entries =
      static_cast<int32_t>(data_use_tab_model_->max_tab_entries_);
  SessionID::id_type tab_id = 1;

  while (tab_id <= max_tab_entries) {
    std::string tab_label = base::StringPrintf("label_%d", tab_id);
    StartTrackingDataUse(tab_id, tab_label);
    EndTrackingDataUse(tab_id);
    ++tab_id;
  }

  // Fast forward 10 minutes.
  tick_clock_->Advance(base::TimeDelta::FromMinutes(10));

  // Adding another tab entry triggers CompactTabEntries.
  std::string tab_label = base::StringPrintf("label_%d", tab_id);
  StartTrackingDataUse(tab_id, tab_label);
  EndTrackingDataUse(tab_id);

  histogram_tester.ExpectUniqueSample(
      kUMAUnexpiredTabEntryRemovalDurationHistogram,
      base::TimeDelta::FromMinutes(10).InMilliseconds(), 1);
}

// Tests that Enter navigation events start tracking the tab entry.
TEST_F(DataUseTabModelTest, NavigationEnterEvent) {
  std::vector<std::string> app_package_names, domain_regexes, labels;

  // Matching rule with app package name.
  app_package_names.push_back(std::string());
  domain_regexes.push_back(kURLFoo);
  labels.push_back(kTestLabel1);

  // Matching rule with regex.
  app_package_names.push_back(std::string());
  domain_regexes.push_back(kURLBar);
  labels.push_back(kTestLabel2);

  RegisterURLRegexes(app_package_names, domain_regexes, labels);

  ExpectTabEntrySize(TabEntrySize::ZERO);

  EXPECT_FALSE(data_use_tab_model_->WouldNavigationEventEndTracking(
      kTabID1, DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION, GURL(kURLFoo)));
  data_use_tab_model_->OnNavigationEvent(
      kTabID1, DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION, GURL(kURLFoo),
      std::string());
  ExpectTabEntrySize(TabEntrySize::ONE);
  EXPECT_TRUE(IsTrackingDataUse(kTabID1));
  ExpectDataUseLabel(kTabID1, kTestLabel1);
  EXPECT_TRUE(data_use_tab_model_->WouldNavigationEventEndTracking(
      kTabID1, DataUseTabModel::TRANSITION_OMNIBOX_SEARCH, GURL(kURLFooBar)));

  EXPECT_FALSE(data_use_tab_model_->WouldNavigationEventEndTracking(
      kTabID2, DataUseTabModel::TRANSITION_OMNIBOX_SEARCH, GURL(kURLBar)));
  data_use_tab_model_->OnNavigationEvent(
      kTabID2, DataUseTabModel::TRANSITION_OMNIBOX_SEARCH, GURL(kURLBar),
      std::string());
  ExpectTabEntrySize(TabEntrySize::TWO);
  EXPECT_TRUE(IsTrackingDataUse(kTabID2));
  ExpectDataUseLabel(kTabID2, kTestLabel2);
  EXPECT_TRUE(data_use_tab_model_->WouldNavigationEventEndTracking(
      kTabID2, DataUseTabModel::TRANSITION_OMNIBOX_SEARCH, GURL(kURLFooBar)));
}

// Tests that a navigation event with empty url and empty package name does not
// start tracking.
TEST_F(DataUseTabModelTest, EmptyNavigationEvent) {
  ExpectTabEntrySize(TabEntrySize::ZERO);

  RegisterURLRegexes(std::vector<std::string>(1, std::string()),
                     std::vector<std::string>(1, kURLFoo),
                     std::vector<std::string>(1, kTestLabel1));

  data_use_tab_model_->OnNavigationEvent(
      kTabID1, DataUseTabModel::TRANSITION_CUSTOM_TAB, GURL(), std::string());
  EXPECT_FALSE(IsTrackingDataUse(kTabID1));

  data_use_tab_model_->OnNavigationEvent(
      kTabID1, DataUseTabModel::TRANSITION_OMNIBOX_SEARCH, GURL(),
      std::string());
  EXPECT_FALSE(IsTrackingDataUse(kTabID1));

  EXPECT_FALSE(data_use_tab_model_->WouldNavigationEventEndTracking(
      kTabID1, DataUseTabModel::TRANSITION_OMNIBOX_SEARCH, GURL()));

  ExpectTabEntrySize(TabEntrySize::ZERO);
}

// Tests that Exit navigation event ends the tracking.
TEST_F(DataUseTabModelTest, NavigationEnterAndExitEvent) {
  std::vector<std::string> app_package_names, domain_regexes, labels;

  app_package_names.push_back(std::string());
  domain_regexes.push_back(kURLFoo);
  labels.push_back(kTestLabel2);

  RegisterURLRegexes(app_package_names, domain_regexes, labels);

  EXPECT_FALSE(data_use_tab_model_->WouldNavigationEventEndTracking(
      kTabID1, DataUseTabModel::TRANSITION_OMNIBOX_SEARCH, GURL(kURLFoo)));
  data_use_tab_model_->OnNavigationEvent(
      kTabID1, DataUseTabModel::TRANSITION_OMNIBOX_SEARCH, GURL(kURLFoo),
      std::string());
  ExpectTabEntrySize(TabEntrySize::ONE);
  EXPECT_TRUE(IsTrackingDataUse(kTabID1));
  EXPECT_TRUE(data_use_tab_model_->WouldNavigationEventEndTracking(
      kTabID1, DataUseTabModel::TRANSITION_BOOKMARK, GURL()));
}

// Tests that any of the Enter transition events start the tracking.
TEST_F(DataUseTabModelTest, AllNavigationEnterEvents) {
  const struct {
    DataUseTabModel::TransitionType transition;
    std::string url;
    std::string package;
    std::string expect_label;
  } all_enter_transition_tests[] = {
      {DataUseTabModel::TRANSITION_CUSTOM_TAB, std::string(), kPackageFoo,
       kTestLabel1},
      {DataUseTabModel::TRANSITION_CUSTOM_TAB, std::string(), kPackageBar,
       kTestLabel3},
      {DataUseTabModel::TRANSITION_OMNIBOX_SEARCH, kURLFoo, std::string(),
       kTestLabel2},
      {DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION, kURLFoo, std::string(),
       kTestLabel2},
      {DataUseTabModel::TRANSITION_LINK, kURLFoo, std::string(), kTestLabel2},
      {DataUseTabModel::TRANSITION_RELOAD, kURLFoo, std::string(), kTestLabel2},
  };
  std::vector<std::string> app_package_names, domain_regexes, labels;
  SessionID::id_type tab_id = 1;

  app_package_names.push_back(kPackageFoo);
  domain_regexes.push_back(std::string());
  labels.push_back(kTestLabel1);
  app_package_names.push_back(std::string());
  domain_regexes.push_back(kURLFoo);
  labels.push_back(kTestLabel2);
  app_package_names.push_back(kPackageBar);
  domain_regexes.push_back(std::string());
  labels.push_back(kTestLabel3);

  RegisterURLRegexes(app_package_names, domain_regexes, labels);

  for (const auto& test : all_enter_transition_tests) {
    EXPECT_FALSE(IsTrackingDataUse(tab_id));
    ExpectEmptyDataUseLabel(tab_id);

    // Tracking should start.
    data_use_tab_model_->OnNavigationEvent(tab_id, test.transition,
                                           GURL(test.url), test.package);

    EXPECT_TRUE(IsTrackingDataUse(tab_id));
    ExpectDataUseLabel(tab_id, test.expect_label);
    if (test.transition != DataUseTabModel::TRANSITION_CUSTOM_TAB) {
      EXPECT_TRUE(data_use_tab_model_->WouldNavigationEventEndTracking(
          tab_id, DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION,
          GURL(kURLBar)));
    }
    ExpectTabEntrySize(tab_id);
    EXPECT_EQ(test.transition == DataUseTabModel::TRANSITION_CUSTOM_TAB,
              IsCustomTabPackageMatch(tab_id));

    ++tab_id;
  }
}

// Tests that any of the Exit transition events end the tracking.
TEST_F(DataUseTabModelTest, AllNavigationExitEvents) {
  const struct {
    DataUseTabModel::TransitionType transition;
    std::string url;
  } all_exit_transition_tests[] = {
      {DataUseTabModel::TRANSITION_BOOKMARK, std::string()},
      {DataUseTabModel::TRANSITION_HISTORY_ITEM, std::string()},
      {DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION, kURLFooBar},
      {DataUseTabModel::TRANSITION_OMNIBOX_SEARCH, kURLFooBar},
  };
  std::vector<std::string> app_package_names, domain_regexes, labels;
  SessionID::id_type tab_id = 1;

  app_package_names.push_back(std::string());
  domain_regexes.push_back(kURLFoo);
  labels.push_back(kTestLabel1);

  RegisterURLRegexes(app_package_names, domain_regexes, labels);

  for (const auto& test : all_exit_transition_tests) {
    EXPECT_FALSE(IsTrackingDataUse(tab_id));
    data_use_tab_model_->OnNavigationEvent(
        tab_id, DataUseTabModel::TRANSITION_OMNIBOX_SEARCH, GURL(kURLFoo),
        std::string());
    EXPECT_TRUE(IsTrackingDataUse(tab_id));

    // Tracking should end.
    EXPECT_TRUE(data_use_tab_model_->WouldNavigationEventEndTracking(
        tab_id, test.transition, GURL(test.url)));
    data_use_tab_model_->OnNavigationEvent(tab_id, test.transition,
                                           GURL(test.url), std::string());
    EXPECT_FALSE(data_use_tab_model_->WouldNavigationEventEndTracking(
        tab_id, test.transition, GURL(test.url)));

    EXPECT_FALSE(IsTrackingDataUse(tab_id));
    ExpectTabEntrySize(tab_id);
    ++tab_id;
  }
}

// Tests that transition events that can be enter or exit, are able to start and
// end the tracking with correct labels.
TEST_F(DataUseTabModelTest, AllNavigationExitAndEnterEvents) {
  const DataUseTabModel::TransitionType all_test_transitions[] = {
      DataUseTabModel::TRANSITION_OMNIBOX_SEARCH,
      DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION};
  std::vector<std::string> app_package_names, domain_regexes, labels;
  SessionID::id_type tab_id = 1;

  app_package_names.push_back(std::string());
  domain_regexes.push_back(kURLFoo);
  labels.push_back(kTestLabel1);

  RegisterURLRegexes(app_package_names, domain_regexes, labels);

  for (const auto& transition : all_test_transitions) {
    EXPECT_FALSE(IsTrackingDataUse(tab_id));
    data_use_tab_model_->OnNavigationEvent(tab_id, transition, GURL(kURLFoo),
                                           std::string());
    EXPECT_TRUE(IsTrackingDataUse(tab_id));
    ExpectDataUseLabel(tab_id, kTestLabel1);
    EXPECT_TRUE(data_use_tab_model_->WouldNavigationEventEndTracking(
        tab_id, DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION, GURL(kURLBar)));

    // No change in label.
    data_use_tab_model_->OnNavigationEvent(tab_id, transition, GURL(kURLFoo),
                                           std::string());
    EXPECT_TRUE(IsTrackingDataUse(tab_id));
    ExpectDataUseLabel(tab_id, kTestLabel1);

    // Tracking should end.
    EXPECT_TRUE(data_use_tab_model_->WouldNavigationEventEndTracking(
        tab_id, transition, GURL()));
    data_use_tab_model_->OnNavigationEvent(tab_id, transition, GURL(),
                                           std::string());

    EXPECT_FALSE(IsTrackingDataUse(tab_id));
    ExpectTabEntrySize(tab_id);
    ++tab_id;
  }
}

// Tests that a sequence of transitions simulating user actions are able to
// start and end the tracking with correct label.
TEST_F(DataUseTabModelTest, SingleTabTransitionSequence) {
  std::vector<std::string> app_package_names, domain_regexes, labels;
  MockTabDataUseObserver mock_observer;

  const struct {
    DataUseTabModel::TransitionType transition;
    std::string url;
    std::string package;
    std::string expected_label;
    TrackingState observer_event;
  } transition_tests[] = {
      // Opening matching URL from omnibox starts tracking.
      {DataUseTabModel::TRANSITION_OMNIBOX_SEARCH, kURLBaz, std::string(),
       kTestLabel1, STARTED},
      // Clicking on links in the page continues tracking.
      {DataUseTabModel::TRANSITION_LINK, kURLBar, std::string(), kTestLabel1,
       CONTINUES},
      {DataUseTabModel::TRANSITION_LINK, kURLFooBar, std::string(), kTestLabel1,
       CONTINUES},
      // Navigating to a non matching URL from omnibox ends tracking.
      {DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION, kURLBar, std::string(),
       std::string(), ENDED},
      // Clicking on non matching URL links in the page does not start tracking.
      {DataUseTabModel::TRANSITION_LINK, kURLFooBar, std::string(),
       std::string(), NONE},
      // Clicking on matching URL links in the page starts tracking.
      {DataUseTabModel::TRANSITION_LINK, kURLFoo, std::string(), kTestLabel2,
       STARTED},
      // Navigating to bookmark with matching URL does not end tracking.
      {DataUseTabModel::TRANSITION_BOOKMARK, kURLFoo, std::string(),
       kTestLabel2, CONTINUES},
      // Navigating to bookmark with non-matching URL ends tracking.
      {DataUseTabModel::TRANSITION_BOOKMARK, std::string(), std::string(),
       std::string(), ENDED},
      // Navigating to a matching URL from omnibox starts tracking.
      {DataUseTabModel::TRANSITION_OMNIBOX_NAVIGATION, kURLFoo, std::string(),
       kTestLabel2, STARTED},
      // Navigating to history item ends tracking.
      {DataUseTabModel::TRANSITION_HISTORY_ITEM, std::string(), std::string(),
       std::string(), ENDED},
  };

  app_package_names.push_back(std::string());
  domain_regexes.push_back(kURLBaz);
  labels.push_back(kTestLabel1);
  app_package_names.push_back(std::string());
  domain_regexes.push_back(kURLFoo);
  labels.push_back(kTestLabel2);
  RegisterURLRegexes(app_package_names, domain_regexes, labels);

  data_use_tab_model_->AddObserver(&mock_observer);
  for (auto const& test : transition_tests) {
    tick_clock_->Advance(base::TimeDelta::FromSeconds(1));

    if (test.observer_event == ENDED) {
      EXPECT_TRUE(data_use_tab_model_->WouldNavigationEventEndTracking(
          kTabID1, test.transition, GURL(test.url)));
    }

    EXPECT_CALL(mock_observer, NotifyTrackingStarting(kTabID1))
        .Times(test.observer_event == STARTED ? 1 : 0);
    EXPECT_CALL(mock_observer, NotifyTrackingEnding(kTabID1))
        .Times(test.observer_event == ENDED ? 1 : 0);

    data_use_tab_model_->OnNavigationEvent(kTabID1, test.transition,
                                           GURL(test.url), test.package);
    tick_clock_->Advance(base::TimeDelta::FromSeconds(1));

    EXPECT_EQ(!test.expected_label.empty(), IsTrackingDataUse(kTabID1));
    ExpectDataUseLabel(kTabID1, test.expected_label);

    if (test.observer_event == STARTED || test.observer_event == CONTINUES) {
      EXPECT_TRUE(data_use_tab_model_->WouldNavigationEventEndTracking(
          kTabID1, DataUseTabModel::TRANSITION_BOOKMARK, GURL()));
    }

    testing::Mock::VerifyAndClearExpectations(&mock_observer);
  }
}

// Tests that a sequence of transitions in a custom tab that has an active
// tracking session never ends the tracking.
TEST_F(DataUseTabModelTest, SingleCustomTabTransitionSequence) {
  std::vector<std::string> app_package_names, domain_regexes, labels;
  MockTabDataUseObserver mock_observer;

  const struct {
    DataUseTabModel::TransitionType transition;
    std::string url;
    std::string package;
    std::string expected_label;
    TrackingState observer_event;
  } transition_tests[] = {
      // Opening Custom Tab with matching package starts tracking.
      {DataUseTabModel::TRANSITION_CUSTOM_TAB, std::string(), kPackageFoo,
       kTestLabel1, STARTED},
      // Clicking on links in the page continues tracking.
      {DataUseTabModel::TRANSITION_LINK, kURLBar, std::string(), kTestLabel1,
       CONTINUES},
      {DataUseTabModel::TRANSITION_LINK, kURLFooBar, std::string(), kTestLabel1,
       CONTINUES},
      // Clicking on bookmark continues tracking.
      {DataUseTabModel::TRANSITION_BOOKMARK, kURLFooBar, std::string(),
       kTestLabel1, CONTINUES},
      // Reloading the page continues tracking.
      {DataUseTabModel::TRANSITION_RELOAD, kURLFooBar, std::string(),
       kTestLabel1, CONTINUES},
      // Clicking on links in the page continues tracking.
      {DataUseTabModel::TRANSITION_LINK, kURLBar, std::string(), kTestLabel1,
       CONTINUES},
  };

  app_package_names.push_back(kPackageFoo);
  domain_regexes.push_back(std::string());
  labels.push_back(kTestLabel1);
  app_package_names.push_back(std::string());
  domain_regexes.push_back(kURLFoo);
  labels.push_back(kTestLabel2);
  RegisterURLRegexes(app_package_names, domain_regexes, labels);

  data_use_tab_model_->AddObserver(&mock_observer);
  for (auto const& test : transition_tests) {
    tick_clock_->Advance(base::TimeDelta::FromSeconds(1));

    EXPECT_CALL(mock_observer, NotifyTrackingStarting(kTabID1))
        .Times(test.observer_event == STARTED ? 1 : 0);
    EXPECT_CALL(mock_observer, NotifyTrackingEnding(kTabID1))
        .Times(test.observer_event == ENDED ? 1 : 0);

    data_use_tab_model_->OnNavigationEvent(kTabID1, test.transition,
                                           GURL(test.url), test.package);
    tick_clock_->Advance(base::TimeDelta::FromSeconds(1));

    EXPECT_EQ(!test.expected_label.empty(), IsTrackingDataUse(kTabID1));
    ExpectDataUseLabel(kTabID1, test.expected_label);

    // Tracking never ends.
    EXPECT_FALSE(data_use_tab_model_->WouldNavigationEventEndTracking(
        kTabID1, DataUseTabModel::TRANSITION_LINK, GURL()));

    testing::Mock::VerifyAndClearExpectations(&mock_observer);
  }
}

// Tests that tab model is notified when tracking labels are removed.
TEST_F(DataUseTabModelTest, LabelRemoved) {
  std::vector<std::string> labels;

  tick_clock_->Advance(base::TimeDelta::FromSeconds(1));
  labels.push_back(kTestLabel1);
  labels.push_back(kTestLabel2);
  labels.push_back(kTestLabel3);
  RegisterURLRegexes(std::vector<std::string>(labels.size(), std::string()),
                     std::vector<std::string>(labels.size(), kURLFoo), labels);

  data_use_tab_model_->OnNavigationEvent(
      kTabID1, DataUseTabModel::TRANSITION_OMNIBOX_SEARCH, GURL(kURLFoo),
      std::string());
  EXPECT_TRUE(IsTrackingDataUse(kTabID1));

  labels.clear();
  labels.push_back(kTestLabel2);
  labels.push_back("label_4");
  labels.push_back("label_5");
  RegisterURLRegexes(std::vector<std::string>(labels.size(), std::string()),
                     std::vector<std::string>(labels.size(), kURLFoo), labels);
  EXPECT_FALSE(IsTrackingDataUse(kTabID1));
}

// Tests the behavior when the external control app is uninstalled. When the app
// gets uninstalled the active tracking sessions should end and the existing
// matching rules should be cleared.
TEST_F(DataUseTabModelTest, MatchingRuleClearedOnControlAppUninstall) {
  std::vector<std::string> app_package_names, domain_regexes, labels;

  app_package_names.push_back(kPackageFoo);
  domain_regexes.push_back(kURLFoo);
  labels.push_back(kTestLabel1);

  RegisterURLRegexes(app_package_names, domain_regexes, labels);

  StartTrackingDataUse(kTabID1, kTestLabel1);
  EXPECT_TRUE(IsTrackingDataUse(kTabID1));
  EXPECT_TRUE(data_use_tab_model_->data_use_matcher_->HasValidRules());

  data_use_tab_model_->OnControlAppInstallStateChange(false);

  EXPECT_FALSE(IsTrackingDataUse(kTabID1));
  EXPECT_FALSE(data_use_tab_model_->data_use_matcher_->HasValidRules());
}

// Tests that |OnDataUseTabModelReady| is sent to observers when the external
// control app not installed callback was received.
TEST_F(DataUseTabModelTest, ReadyForNavigationEventWhenControlAppNotInstalled) {
  MockTabDataUseObserver mock_observer;
  data_use_tab_model_->AddObserver(&mock_observer);

  EXPECT_FALSE(data_use_tab_model_->is_ready_for_navigation_event());
  EXPECT_CALL(mock_observer, OnDataUseTabModelReady()).Times(1);

  data_use_tab_model_->OnControlAppInstallStateChange(false);
  EXPECT_TRUE(data_use_tab_model_->is_ready_for_navigation_event());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Subsequent install and uninstall of the control app does not trigger the
  // event.
  EXPECT_CALL(mock_observer, OnDataUseTabModelReady()).Times(0);
  data_use_tab_model_->OnControlAppInstallStateChange(true);
  data_use_tab_model_->OnControlAppInstallStateChange(false);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_TRUE(data_use_tab_model_->is_ready_for_navigation_event());
}

// Tests that |OnDataUseTabModelReady| is sent to observers when the first rule
// fetch is complete.
TEST_F(DataUseTabModelTest, ReadyForNavigationEventAfterRuleFetch) {
  MockTabDataUseObserver mock_observer;
  std::vector<std::string> app_package_names, domain_regexes, labels;

  app_package_names.push_back(kPackageFoo);
  domain_regexes.push_back(kURLFoo);
  labels.push_back(kTestLabel1);
  data_use_tab_model_->AddObserver(&mock_observer);

  EXPECT_FALSE(data_use_tab_model_->is_ready_for_navigation_event());
  EXPECT_CALL(mock_observer, OnDataUseTabModelReady()).Times(1);

  // First rule fetch triggers the event.
  RegisterURLRegexes(app_package_names, domain_regexes, labels);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Subsequent rule fetches, uninstall and install of the control app does not
  // trigger the event.
  EXPECT_CALL(mock_observer, OnDataUseTabModelReady()).Times(0);
  RegisterURLRegexes(app_package_names, domain_regexes, labels);
  data_use_tab_model_->OnControlAppInstallStateChange(false);
  data_use_tab_model_->OnControlAppInstallStateChange(true);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_TRUE(data_use_tab_model_->is_ready_for_navigation_event());
}

}  // namespace android

}  // namespace chrome
