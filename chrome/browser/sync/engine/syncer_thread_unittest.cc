// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <map>
#include <set>
#include <strstream>

#include "base/scoped_ptr.h"
#include "chrome/browser/sync/engine/syncer_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

class SyncerThreadTest : public testing::Test {
 protected:
  SyncerThreadTest() {}
  virtual ~SyncerThreadTest() {}
  virtual void SetUp() {}
  virtual void TearDown() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncerThreadTest);
};

TEST_F(SyncerThreadTest, Construction) {
  SyncerThread syncer_thread(NULL, NULL, NULL, NULL, NULL);
}

TEST_F(SyncerThreadTest, CalculateSyncWaitTime) {
  SyncerThread syncer_thread(NULL, NULL, NULL, NULL, NULL);
  syncer_thread.DisableIdleDetection();

  // Syncer_polling_interval_ is less than max poll interval
  int syncer_polling_interval = 1;  // Needed since AssertionResult is not a
                                    // friend of SyncerThread
  syncer_thread.syncer_polling_interval_ = syncer_polling_interval;

  // user_idle_ms is less than 10 * (syncer_polling_interval*1000).
  ASSERT_EQ(syncer_polling_interval * 1000,
            syncer_thread.CalculateSyncWaitTime(1000, 0));
  ASSERT_EQ(syncer_polling_interval * 1000,
            syncer_thread.CalculateSyncWaitTime(1000, 1));

  // user_idle_ms is ge than 10 * (syncer_polling_interval*1000).
  int last_poll_time = 2000;
  ASSERT_LE(last_poll_time,
            syncer_thread.CalculateSyncWaitTime(last_poll_time, 10000));
  ASSERT_GE(last_poll_time*3,
            syncer_thread.CalculateSyncWaitTime(last_poll_time, 10000));
  ASSERT_LE(last_poll_time,
            syncer_thread.CalculateSyncWaitTime(last_poll_time, 100000));
  ASSERT_GE(last_poll_time*3,
            syncer_thread.CalculateSyncWaitTime(last_poll_time, 100000));

  // Maximum backoff time should be syncer_max_interval.
  int near_threshold = SyncerThread::kDefaultMaxPollIntervalMs / 2 - 1;
  int threshold = SyncerThread::kDefaultMaxPollIntervalMs;
  int over_threshold = SyncerThread::kDefaultMaxPollIntervalMs + 1;
  ASSERT_LE(near_threshold,
            syncer_thread.CalculateSyncWaitTime(near_threshold, 10000));
  ASSERT_GE(SyncerThread::kDefaultMaxPollIntervalMs,
            syncer_thread.CalculateSyncWaitTime(near_threshold, 10000));
  ASSERT_EQ(SyncerThread::kDefaultMaxPollIntervalMs,
            syncer_thread.CalculateSyncWaitTime(threshold, 10000));
  ASSERT_EQ(SyncerThread::kDefaultMaxPollIntervalMs,
            syncer_thread.CalculateSyncWaitTime(over_threshold, 10000));

  // Possible idle time must be capped by syncer_max_interval.
  int over_sync_max_interval =
      SyncerThread::kDefaultMaxPollIntervalMs + 1;
  syncer_polling_interval = over_sync_max_interval / 100;  // so 1000* is right
  syncer_thread.syncer_polling_interval_ = syncer_polling_interval;
  ASSERT_EQ(syncer_polling_interval * 1000,
            syncer_thread.CalculateSyncWaitTime(1000, over_sync_max_interval));
  syncer_polling_interval = 1;
  syncer_thread.syncer_polling_interval_ = syncer_polling_interval;
  ASSERT_LE(last_poll_time,
            syncer_thread.CalculateSyncWaitTime(last_poll_time,
                                                over_sync_max_interval));
  ASSERT_GE(last_poll_time * 3,
            syncer_thread.CalculateSyncWaitTime(last_poll_time,
                                                over_sync_max_interval));
}

TEST_F(SyncerThreadTest, CalculatePollingWaitTime) {
  // Set up the environment.
  int user_idle_milliseconds_param = 0;

  SyncerThread syncer_thread(NULL, NULL, NULL, NULL, NULL);
  syncer_thread.DisableIdleDetection();

  // Notifications disabled should result in a polling interval of
  // kDefaultShortPollInterval.
  {
    AllStatus::Status status = {};
    status.notifications_enabled = 0;
    bool continue_sync_cycle_param = false;

    // No work and no backoff.
    ASSERT_EQ(SyncerThread::kDefaultShortPollIntervalSeconds,
              syncer_thread.CalculatePollingWaitTime(
                  status,
                  0,
                  &user_idle_milliseconds_param,
                  &continue_sync_cycle_param));
    ASSERT_FALSE(continue_sync_cycle_param);

    // In this case the continue_sync_cycle is turned off.
    continue_sync_cycle_param = true;
    ASSERT_EQ(SyncerThread::kDefaultShortPollIntervalSeconds,
              syncer_thread.CalculatePollingWaitTime(
                  status,
                  0,
                  &user_idle_milliseconds_param,
                  &continue_sync_cycle_param));
    ASSERT_FALSE(continue_sync_cycle_param);

    // TODO(brg) : Find a way to test exponential backoff is inoperable.
    // Exponential backoff should be turned on when notifications are disabled
    // but this can not be tested since we can not set the last input info.
  }

  // Notifications enabled should result in a polling interval of
  // SyncerThread::kDefaultLongPollIntervalSeconds.
  {
    AllStatus::Status status = {};
    status.notifications_enabled = 1;
    bool continue_sync_cycle_param = false;

    // No work and no backoff.
    ASSERT_EQ(SyncerThread::kDefaultLongPollIntervalSeconds,
              syncer_thread.CalculatePollingWaitTime(
                  status,
                  0,
                  &user_idle_milliseconds_param,
                  &continue_sync_cycle_param));
    ASSERT_FALSE(continue_sync_cycle_param);

    // In this case the continue_sync_cycle is turned off.
    continue_sync_cycle_param = true;
    ASSERT_EQ(SyncerThread::kDefaultLongPollIntervalSeconds,
              syncer_thread.CalculatePollingWaitTime(
                  status,
                  0,
                  &user_idle_milliseconds_param,
                  &continue_sync_cycle_param));
    ASSERT_FALSE(continue_sync_cycle_param);

    // TODO(brg) : Find a way to test exponential backoff.
    // Exponential backoff should be turned off when notifications are enabled,
    // but this can not be tested since we can not set the last input info.
  }

  // There are two states which can cause a continuation, either the updates
  // available do not match the updates received, or the unsynced count is
  // non-zero.
  {
    AllStatus::Status status = {};
    status.updates_available = 1;
    status.updates_received = 0;
    bool continue_sync_cycle_param = false;

    ASSERT_LE(0, syncer_thread.CalculatePollingWaitTime(
                     status,
                     0,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    continue_sync_cycle_param = false;
    ASSERT_GE(3, syncer_thread.CalculatePollingWaitTime(
                     status,
                     0,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    ASSERT_LE(0, syncer_thread.CalculatePollingWaitTime(
                     status,
                     0,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_GE(2, syncer_thread.CalculatePollingWaitTime(
                     status,
                     0,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    status.updates_received = 1;
    ASSERT_EQ(SyncerThread::kDefaultShortPollIntervalSeconds,
              syncer_thread.CalculatePollingWaitTime(
                  status,
                  10,
                  &user_idle_milliseconds_param,
                  &continue_sync_cycle_param));
    ASSERT_FALSE(continue_sync_cycle_param);
  }

  {
    AllStatus::Status status = {};
    status.unsynced_count = 1;
    bool continue_sync_cycle_param = false;

    ASSERT_LE(0, syncer_thread.CalculatePollingWaitTime(
                     status,
                     0,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    continue_sync_cycle_param = false;
    ASSERT_GE(2, syncer_thread.CalculatePollingWaitTime(
                     status,
                     0,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    status.unsynced_count = 0;
    ASSERT_EQ(SyncerThread::kDefaultShortPollIntervalSeconds,
              syncer_thread.CalculatePollingWaitTime(
                  status,
                  4,
                  &user_idle_milliseconds_param,
                  &continue_sync_cycle_param));
    ASSERT_FALSE(continue_sync_cycle_param);
  }

  // Regression for exponential backoff reset when the syncer is nudged.
  {
    AllStatus::Status status = {};
    status.unsynced_count = 1;
    bool continue_sync_cycle_param = false;

    // Expect move from default polling interval to exponential backoff due to
    // unsynced_count != 0.
    ASSERT_LE(0, syncer_thread.CalculatePollingWaitTime(
                     status,
                     3600,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    continue_sync_cycle_param = false;
    ASSERT_GE(2, syncer_thread.CalculatePollingWaitTime(
                     status,
                     3600,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    // Expect exponential backoff.
    ASSERT_LE(2, syncer_thread.CalculatePollingWaitTime(
                     status,
                     2,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_GE(6, syncer_thread.CalculatePollingWaitTime(
                     status,
                     2,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    // A nudge resets the continue_sync_cycle_param value, so our backoff
    // should return to the minimum.
    continue_sync_cycle_param = false;
    ASSERT_LE(0, syncer_thread.CalculatePollingWaitTime(
                     status,
                     3600,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    continue_sync_cycle_param = false;
    ASSERT_GE(2, syncer_thread.CalculatePollingWaitTime(
                     status,
                     3600,
                     &user_idle_milliseconds_param,
                     &continue_sync_cycle_param));
    ASSERT_TRUE(continue_sync_cycle_param);

    // Setting unsynced_count = 0 returns us to the default polling interval.
    status.unsynced_count = 0;
    ASSERT_EQ(SyncerThread::kDefaultShortPollIntervalSeconds,
              syncer_thread.CalculatePollingWaitTime(
                  status,
                  4,
                  &user_idle_milliseconds_param,
                  &continue_sync_cycle_param));
    ASSERT_FALSE(continue_sync_cycle_param);
  }
}

}  // namespace browser_sync
