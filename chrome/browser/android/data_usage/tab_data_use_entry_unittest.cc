// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/data_usage/tab_data_use_entry.h"

#include <stdint.h>

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Tracking labels for tests.
const std::string kTestLabel1 = "label_1";
const std::string kTestLabel2 = "label_2";
const std::string kTestLabel3 = "label_3";

enum TabEntrySessionSize { ZERO = 0, ONE, TWO, THREE };

}  // namespace

namespace chrome {

namespace android {

class TabDataUseEntryTest : public testing::Test {
 public:
  TabDataUseEntryTest() { tab_entry_.reset(new TabDataUseEntry()); }

  // Checks if there are |expected_size| tracking session entries in
  // |sessions_|.
  void ExpectTabEntrySessionsSize(uint32_t expected_size) const {
    EXPECT_EQ(expected_size, tab_entry_->sessions_.size());
  }

  scoped_ptr<TabDataUseEntry> tab_entry_;
};

// Starts a single tracking session and checks if a new active session is added
// to the deque. Ends the session and checks if it becomes inactive.
TEST_F(TabDataUseEntryTest, SingleTabSessionStartEnd) {
  ExpectTabEntrySessionsSize(TabEntrySessionSize::ZERO);
  EXPECT_FALSE(tab_entry_->IsTrackingDataUse());

  EXPECT_TRUE(tab_entry_->StartTracking(kTestLabel1));
  ExpectTabEntrySessionsSize(TabEntrySessionSize::ONE);
  EXPECT_TRUE(tab_entry_->IsTrackingDataUse());

  EXPECT_TRUE(tab_entry_->EndTracking());
  ExpectTabEntrySessionsSize(TabEntrySessionSize::ONE);
  EXPECT_FALSE(tab_entry_->IsTrackingDataUse());
}

// Starts multiple tracking sessions and checks if new active sessions are added
// to the deque for each. Ends the sessions and checks if they become inactive.
TEST_F(TabDataUseEntryTest, MultipleTabSessionStartEnd) {
  ExpectTabEntrySessionsSize(TabEntrySessionSize::ZERO);
  EXPECT_FALSE(tab_entry_->IsTrackingDataUse());

  EXPECT_TRUE(tab_entry_->StartTracking(kTestLabel1));
  EXPECT_TRUE(tab_entry_->IsTrackingDataUse());
  EXPECT_TRUE(tab_entry_->EndTracking());
  EXPECT_FALSE(tab_entry_->IsTrackingDataUse());

  EXPECT_TRUE(tab_entry_->StartTracking(kTestLabel2));
  EXPECT_TRUE(tab_entry_->IsTrackingDataUse());
  EXPECT_TRUE(tab_entry_->EndTracking());
  EXPECT_FALSE(tab_entry_->IsTrackingDataUse());

  EXPECT_TRUE(tab_entry_->StartTracking(kTestLabel3));
  EXPECT_TRUE(tab_entry_->IsTrackingDataUse());
  EXPECT_TRUE(tab_entry_->EndTracking());
  EXPECT_FALSE(tab_entry_->IsTrackingDataUse());

  ExpectTabEntrySessionsSize(TabEntrySessionSize::THREE);
}

// Checks that starting a session while a tracking session is already active and
// ending a session while no tracking session is active return false.
TEST_F(TabDataUseEntryTest, DuplicateTabSessionStartEnd) {
  EXPECT_TRUE(tab_entry_->StartTracking(kTestLabel1));
  EXPECT_TRUE(tab_entry_->IsTrackingDataUse());

  // Duplicate start tracking returns false.
  EXPECT_FALSE(tab_entry_->StartTracking(kTestLabel2));
  ExpectTabEntrySessionsSize(TabEntrySessionSize::ONE);
  EXPECT_TRUE(tab_entry_->IsTrackingDataUse());

  EXPECT_TRUE(tab_entry_->EndTracking());
  EXPECT_FALSE(tab_entry_->IsTrackingDataUse());

  // Duplicate end tracking returns false.
  EXPECT_FALSE(tab_entry_->EndTracking());
  EXPECT_FALSE(tab_entry_->IsTrackingDataUse());
  ExpectTabEntrySessionsSize(TabEntrySessionSize::ONE);
}

// Checks that tab close time is updated on a close event for a single tracking
// session.
TEST_F(TabDataUseEntryTest, SingleTabSessionCloseEvent) {
  ExpectTabEntrySessionsSize(TabEntrySessionSize::ZERO);
  EXPECT_TRUE(tab_entry_->tab_close_time_.is_null());

  EXPECT_TRUE(tab_entry_->StartTracking(kTestLabel1));
  EXPECT_TRUE(tab_entry_->tab_close_time_.is_null());

  EXPECT_TRUE(tab_entry_->EndTracking());
  EXPECT_TRUE(tab_entry_->tab_close_time_.is_null());

  base::TimeTicks time_before_close = base::TimeTicks::Now();
  tab_entry_->OnTabCloseEvent();
  EXPECT_FALSE(tab_entry_->tab_close_time_.is_null());
  EXPECT_GE(tab_entry_->tab_close_time_, time_before_close);
  EXPECT_LE(tab_entry_->tab_close_time_, base::TimeTicks::Now());

  EXPECT_FALSE(tab_entry_->IsTrackingDataUse());
}

// Checks that tab close time is updated on a close event after multiple
// tracking sessions.
TEST_F(TabDataUseEntryTest, MultipleTabSessionCloseEvent) {
  ExpectTabEntrySessionsSize(TabEntrySessionSize::ZERO);
  EXPECT_TRUE(tab_entry_->tab_close_time_.is_null());

  EXPECT_TRUE(tab_entry_->StartTracking(kTestLabel1));
  EXPECT_TRUE(tab_entry_->tab_close_time_.is_null());
  EXPECT_TRUE(tab_entry_->EndTracking());
  EXPECT_TRUE(tab_entry_->tab_close_time_.is_null());

  EXPECT_TRUE(tab_entry_->StartTracking(kTestLabel2));
  EXPECT_TRUE(tab_entry_->tab_close_time_.is_null());
  EXPECT_TRUE(tab_entry_->EndTracking());
  EXPECT_TRUE(tab_entry_->tab_close_time_.is_null());

  base::TimeTicks time_before_close = base::TimeTicks::Now();
  tab_entry_->OnTabCloseEvent();
  EXPECT_FALSE(tab_entry_->tab_close_time_.is_null());
  EXPECT_GE(tab_entry_->tab_close_time_, time_before_close);
  EXPECT_LE(tab_entry_->tab_close_time_, base::TimeTicks::Now());

  EXPECT_FALSE(tab_entry_->IsTrackingDataUse());
}

// Test version of |TabDataUseEntry|, which permits mocking of calls to Now.
class MockTabDataUseEntry : public TabDataUseEntry {
 public:
  MockTabDataUseEntry() {}

  ~MockTabDataUseEntry() override {}

  // Returns the mocked Now time after adding |now_offset_seconds| seconds.
  static base::TimeTicks GetNowWithOffset(uint32_t now_offset_seconds) {
    return base::TimeTicks::UnixEpoch() +
           base::TimeDelta::FromSeconds(now_offset_seconds);
  }

  // Sets the |now_offset_| delta to |now_offset_seconds| seconds.
  void SetNowOffsetInSeconds(uint32_t now_offset_seconds) {
    now_offset_ = base::TimeDelta::FromSeconds(now_offset_seconds);
  }

 private:
  // Returns the mocked time as Now.
  base::TimeTicks Now() const override {
    return base::TimeTicks::UnixEpoch() + now_offset_;
  }

  // Represents the delta offset to be added to current time that is returned by
  // Now.
  base::TimeDelta now_offset_;
};

class MockTabDataUseEntryTest : public testing::Test {
 public:
  MockTabDataUseEntryTest() { tab_entry_.reset(new MockTabDataUseEntry()); }

  static unsigned int GetClosedTabExpirationDurationSecondsForTests() {
    return TabDataUseEntry::GetClosedTabExpirationDurationSecondsForTests();
  }

  static unsigned int GetOpenTabExpirationDurationSecondsForTests() {
    return TabDataUseEntry::GetOpenTabExpirationDurationSecondsForTests();
  }

  // Checks if there are |expected_size| tracking session entries in
  // |sessions_|.
  void ExpectTabEntrySessionsSize(uint32_t expected_size) const {
    EXPECT_EQ(expected_size, tab_entry_->sessions_.size());
  }

  // Checks if the tracking session at position |index| in the |sessions_| queue
  // has the start and end times represented by offsets in seconds
  // |start_time_offset| and |end_time_offset| respectively.
  void ExpectStartEndTimeOffsets(uint32_t index,
                                 int start_time_offset,
                                 int end_time_offset) const {
    EXPECT_LT(index, tab_entry_->sessions_.size());
    const TabDataUseTrackingSession& session = tab_entry_->sessions_[index];
    EXPECT_EQ(session.start_time,
              MockTabDataUseEntry::GetNowWithOffset(start_time_offset));
    EXPECT_EQ(session.end_time,
              MockTabDataUseEntry::GetNowWithOffset(end_time_offset));
  }

  // Checks if the data use time at |offset_seconds| is labeled as
  // |expected_label|.
  void ExpectDataUseLabelAtOffsetTime(int offset_seconds,
                                      const std::string& expected_label) const {
    ExpectDataUseLabelAtOffsetTimeWithReturn(offset_seconds, expected_label,
                                             true);
  }

  // Checks if the data use time at |offset_seconds| is labeled as an empty
  // string.
  void ExpectEmptyDataUseLabelAtOffsetTime(int offset_seconds) const {
    ExpectDataUseLabelAtOffsetTimeWithReturn(offset_seconds, std::string(),
                                             false);
  }

  // Checks if GetLabel labels the data use time at |offset_seconds| as
  // |expected_label| and returns |expected_return|.
  void ExpectDataUseLabelAtOffsetTimeWithReturn(
      int offset_seconds,
      const std::string& expected_label,
      bool expected_return) const {
    base::TimeTicks data_use_time =
        MockTabDataUseEntry::GetNowWithOffset(offset_seconds);
    std::string actual_label;
    bool actual_return = tab_entry_->GetLabel(data_use_time, &actual_label);
    EXPECT_EQ(expected_return, actual_return);
    EXPECT_EQ(expected_label, actual_label);
  }

  // Returns true if a tracking session is found labeled with |label|.
  bool IsTabEntrySessionExists(const std::string& label) const {
    for (auto session : tab_entry_->sessions_) {
      if (session.label == label)
        return true;
    }
    return false;
  }

  scoped_ptr<MockTabDataUseEntry> tab_entry_;
};

// Checks that start and end times of tracking sessions are updated for multiple
// tracking sessions.
TEST_F(MockTabDataUseEntryTest, TabSessionStartEndTimes) {
  // Start a tracking session at time=0.
  tab_entry_->SetNowOffsetInSeconds(0);
  EXPECT_TRUE(tab_entry_->StartTracking(kTestLabel1));

  // End the tracking session at time=10.
  tab_entry_->SetNowOffsetInSeconds(10);
  EXPECT_TRUE(tab_entry_->EndTracking());

  // Start a tracking session at time=20, and end it at time=30.
  tab_entry_->SetNowOffsetInSeconds(20);
  EXPECT_TRUE(tab_entry_->StartTracking(kTestLabel2));
  tab_entry_->SetNowOffsetInSeconds(30);
  EXPECT_TRUE(tab_entry_->EndTracking());

  ExpectTabEntrySessionsSize(TabEntrySessionSize::TWO);
  ExpectStartEndTimeOffsets(0, 0, 10);
  ExpectStartEndTimeOffsets(1, 20, 30);
}

// Checks that correct labels are returned for various mock data use times in a
// multiple tracking session scenario.
TEST_F(MockTabDataUseEntryTest, TabSessionLabelDataUse) {
  // No active session.
  ExpectEmptyDataUseLabelAtOffsetTime(0);
  ExpectEmptyDataUseLabelAtOffsetTime(40);

  // Start a tracking session at time=10, and end it at time=20.
  tab_entry_->SetNowOffsetInSeconds(10);
  EXPECT_TRUE(tab_entry_->StartTracking(kTestLabel1));
  tab_entry_->SetNowOffsetInSeconds(20);
  EXPECT_TRUE(tab_entry_->EndTracking());

  // No tracking session active during the time interval [0-10).
  ExpectEmptyDataUseLabelAtOffsetTime(0);
  ExpectEmptyDataUseLabelAtOffsetTime(9);

  // Tracking session active during the time interval [10-20].
  ExpectDataUseLabelAtOffsetTime(10, kTestLabel1);
  ExpectDataUseLabelAtOffsetTime(15, kTestLabel1);
  ExpectDataUseLabelAtOffsetTime(20, kTestLabel1);

  // No tracking session active after time=20.
  ExpectEmptyDataUseLabelAtOffsetTime(21);

  // Start a tracking session at time=30, and end it at time=40.
  tab_entry_->SetNowOffsetInSeconds(30);
  EXPECT_TRUE(tab_entry_->StartTracking(kTestLabel2));
  tab_entry_->SetNowOffsetInSeconds(40);
  EXPECT_TRUE(tab_entry_->EndTracking());

  // Tracking session active during the time interval [10-20] and [30-40].
  ExpectEmptyDataUseLabelAtOffsetTime(0);
  ExpectEmptyDataUseLabelAtOffsetTime(9);
  ExpectDataUseLabelAtOffsetTime(10, kTestLabel1);
  ExpectDataUseLabelAtOffsetTime(15, kTestLabel1);
  ExpectDataUseLabelAtOffsetTime(20, kTestLabel1);
  ExpectEmptyDataUseLabelAtOffsetTime(21);
  ExpectEmptyDataUseLabelAtOffsetTime(29);
  ExpectDataUseLabelAtOffsetTime(30, kTestLabel2);
  ExpectDataUseLabelAtOffsetTime(35, kTestLabel2);
  ExpectDataUseLabelAtOffsetTime(40, kTestLabel2);

  // No tracking session active after time=40.
  ExpectEmptyDataUseLabelAtOffsetTime(41);
}

// Checks that open tab entries expire after
// GetOpenTabExpirationDurationSecondsForTests seconds from their latest
// tracking session start time.
TEST_F(MockTabDataUseEntryTest, OpenTabSessionExpiryFromLatestSessionStart) {
  const unsigned int open_tab_expiration_seconds =
      GetOpenTabExpirationDurationSecondsForTests();

  // Initial tab entry with no sessions is considered expired.
  EXPECT_TRUE(tab_entry_->IsExpired());

  // Start a tracking session at time=10.
  tab_entry_->SetNowOffsetInSeconds(10);
  EXPECT_TRUE(tab_entry_->StartTracking(kTestLabel1));
  EXPECT_FALSE(tab_entry_->IsExpired());

  // Fast forward |open_tab_expiration_seconds| seconds from session start time.
  // Tab entry is not expired.
  tab_entry_->SetNowOffsetInSeconds(10 + open_tab_expiration_seconds);
  EXPECT_FALSE(tab_entry_->IsExpired());

  // Fast forward |open_tab_expiration_seconds+1| seconds from session start
  // time. Tab entry is expired.
  tab_entry_->SetNowOffsetInSeconds(10 + open_tab_expiration_seconds + 1);
  EXPECT_TRUE(tab_entry_->IsExpired());
}

// Checks that open tab entries expire after
// GetOpenTabExpirationDurationSecondsForTests seconds from their latest
// tracking session end time.
TEST_F(MockTabDataUseEntryTest, OpenTabSessionExpiryFromLatestSessionEnd) {
  const unsigned int open_tab_expiration_seconds =
      GetOpenTabExpirationDurationSecondsForTests();

  // Initial tab entry with no sessions is considered expired.
  EXPECT_TRUE(tab_entry_->IsExpired());

  // Start a tracking session at time=10, and end it at time=20.
  tab_entry_->SetNowOffsetInSeconds(10);
  EXPECT_TRUE(tab_entry_->StartTracking(kTestLabel1));
  tab_entry_->SetNowOffsetInSeconds(20);
  EXPECT_TRUE(tab_entry_->EndTracking());
  EXPECT_FALSE(tab_entry_->IsExpired());

  // Fast forward |open_tab_expiration_seconds| seconds from session end
  // time. Tab entry is not expired.
  tab_entry_->SetNowOffsetInSeconds(20 + open_tab_expiration_seconds);
  EXPECT_FALSE(tab_entry_->IsExpired());

  // Fast forward |open_tab_expiration_seconds+1| seconds from session end
  // time. Tab entry is expired.
  tab_entry_->SetNowOffsetInSeconds(20 + open_tab_expiration_seconds + 1);
  EXPECT_TRUE(tab_entry_->IsExpired());
}

// Checks that closed tab entries expire after
// GetClosedTabExpirationDurationSecondsForTests seconds from their closing
// time.
TEST_F(MockTabDataUseEntryTest, ClosedTabSessionExpiry) {
  const unsigned int closed_tab_expiration_seconds =
      GetClosedTabExpirationDurationSecondsForTests();

  // Initial tab entry with no sessions is considered expired.
  EXPECT_TRUE(tab_entry_->IsExpired());

  // Start a tracking session at time=10, and end it at time=20.
  tab_entry_->SetNowOffsetInSeconds(10);
  EXPECT_TRUE(tab_entry_->StartTracking(kTestLabel1));
  tab_entry_->SetNowOffsetInSeconds(20);
  EXPECT_TRUE(tab_entry_->EndTracking());
  EXPECT_FALSE(tab_entry_->IsExpired());

  // Close the tab entry at time=30.
  tab_entry_->SetNowOffsetInSeconds(30);
  tab_entry_->OnTabCloseEvent();

  // Fast forward |closed_tab_expiration_seconds| seconds from tab close
  // time. Tab entry is not expired.
  tab_entry_->SetNowOffsetInSeconds(30 + closed_tab_expiration_seconds);
  EXPECT_FALSE(tab_entry_->IsExpired());

  // Fast forward |closed_tab_expiration_seconds+1| seconds from tab close
  // time. Tab entry is expired.
  tab_entry_->SetNowOffsetInSeconds(30 + closed_tab_expiration_seconds + 1);
  EXPECT_TRUE(tab_entry_->IsExpired());
}

// Checks that tracking session history does not grow beyond
// GetMaxSessionsPerTabForTests entries, and automatically compacts itself by
// removing the oldest tracking sessions.
TEST_F(MockTabDataUseEntryTest, CompactTabSessionHistory) {
  const size_t max_sessions_per_tab =
      TabDataUseEntry::GetMaxSessionsPerTabForTests();
  const uint32_t per_session_duration = 10;
  const uint32_t next_session_start_gap = 10;
  uint32_t session_start_time = 10;
  uint32_t num_sessions = 1;

  ExpectTabEntrySessionsSize(TabEntrySessionSize::ZERO);

  while (num_sessions <= max_sessions_per_tab) {
    // Start tracking session at time=|session_start_time| and end after
    // time=|per_session_duration|.
    std::string session_label = base::StringPrintf("label_%d", num_sessions);
    tab_entry_->SetNowOffsetInSeconds(session_start_time);
    EXPECT_TRUE(tab_entry_->StartTracking(session_label));
    tab_entry_->SetNowOffsetInSeconds(session_start_time +
                                      per_session_duration);
    EXPECT_TRUE(tab_entry_->EndTracking());

    ExpectTabEntrySessionsSize(num_sessions);

    // Update next session start time.
    session_start_time += per_session_duration + next_session_start_gap;
    ++num_sessions;
  }

  int oldest_session = 1;  // Oldest session ID that will be removed first.

  // Check if session history size stays at |max_sessions_per_tab|, when more
  // sessions are added.
  while (num_sessions < max_sessions_per_tab + 10) {
    std::string oldest_session_label =
        base::StringPrintf("label_%d", oldest_session);
    EXPECT_TRUE(IsTabEntrySessionExists(oldest_session_label));

    // Start tracking session at time=|session_start_time| and end after
    // time=|per_session_duration|.
    std::string session_label = base::StringPrintf("label_%d", num_sessions);
    tab_entry_->SetNowOffsetInSeconds(session_start_time);
    EXPECT_TRUE(tab_entry_->StartTracking(session_label));
    tab_entry_->SetNowOffsetInSeconds(session_start_time +
                                      per_session_duration);
    EXPECT_TRUE(tab_entry_->EndTracking());

    // Oldest entry got removed.
    EXPECT_FALSE(IsTabEntrySessionExists(oldest_session_label));
    ExpectTabEntrySessionsSize(max_sessions_per_tab);

    // Update next session start time.
    session_start_time += per_session_duration + next_session_start_gap;
    ++num_sessions;
    ++oldest_session;
  }
}

}  // namespace android

}  // namespace chrome
