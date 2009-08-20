// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/load_log_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

base::TimeTicks MakeTime(int t) {
  base::TimeTicks ticks;  // initialized to 0.
  ticks += base::TimeDelta::FromMilliseconds(t);
  return ticks;
}

void ExpectLogContains(const LoadLog* log,
                       size_t i,
                       LoadLog::EventType expected_event,
                       LoadLog::EventPhase expected_phase) {
  ASSERT_LT(i, log->events().size());
  EXPECT_EQ(expected_event, log->events()[i].type);
  EXPECT_EQ(expected_phase, log->events()[i].phase);
}

void ExpectLogContains(const LoadLog* log,
                       size_t i,
                       base::TimeTicks expected_time,
                       LoadLog::EventType expected_event,
                       LoadLog::EventPhase expected_phase) {
  ASSERT_LT(i, log->events().size());
  EXPECT_TRUE(expected_time == log->events()[i].time);
  EXPECT_EQ(expected_event, log->events()[i].type);
  EXPECT_EQ(expected_phase, log->events()[i].phase);
}

namespace {

TEST(LoadLogTest, Nullable) {
  // Make sure that the static methods can be called with NULL (no-op).
  // (Should not crash).
  LoadLog::BeginEvent(NULL, LoadLog::TYPE_HOST_RESOLVER_IMPL);
  LoadLog::AddEvent(NULL, LoadLog::TYPE_HOST_RESOLVER_IMPL);
  LoadLog::EndEvent(NULL, LoadLog::TYPE_HOST_RESOLVER_IMPL);
}

TEST(LoadLogTest, Basic) {
  scoped_refptr<LoadLog> log(new LoadLog);

  // Logs start off empty.
  EXPECT_EQ(0u, log->events().size());

  // Add 3 entries.

  log->Add(MakeTime(0), LoadLog::TYPE_HOST_RESOLVER_IMPL, LoadLog::PHASE_BEGIN);
  log->Add(MakeTime(2), LoadLog::TYPE_CANCELLED, LoadLog::PHASE_NONE);
  log->Add(MakeTime(11), LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART,
           LoadLog::PHASE_END);

  EXPECT_EQ(3u, log->events().size());

  ExpectLogContains(log, 0, MakeTime(0), LoadLog::TYPE_HOST_RESOLVER_IMPL,
                    LoadLog::PHASE_BEGIN);

  ExpectLogContains(log, 1, MakeTime(2), LoadLog::TYPE_CANCELLED,
                    LoadLog::PHASE_NONE);

  ExpectLogContains(log, 2, MakeTime(11),
                    LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART,
                    LoadLog::PHASE_END);
}

// Test that the log's size is strictly bounded.
TEST(LoadLogTest, Truncation) {
  scoped_refptr<LoadLog> log(new LoadLog);

  // Almost max it out.
  for (size_t i = 0; i < LoadLog::kMaxNumEntries - 1; ++i) {
    log->Add(MakeTime(0), LoadLog::TYPE_CANCELLED, LoadLog::PHASE_NONE);
  }
  EXPECT_EQ(LoadLog::kMaxNumEntries - 1,
            static_cast<int>(log->events().size()));
  EXPECT_EQ(LoadLog::TYPE_CANCELLED,
            log->events()[LoadLog::kMaxNumEntries - 2].type);

  // Max it out (none of these get appended to the log).
  log->Add(MakeTime(0), LoadLog::TYPE_CANCELLED, LoadLog::PHASE_NONE);
  log->Add(MakeTime(0), LoadLog::TYPE_CANCELLED, LoadLog::PHASE_NONE);
  log->Add(MakeTime(0), LoadLog::TYPE_CANCELLED, LoadLog::PHASE_NONE);

  EXPECT_EQ(LoadLog::kMaxNumEntries, log->events().size());

  // We terminated with a "truncation" event.
  ExpectLogContains(log, LoadLog::kMaxNumEntries - 1, MakeTime(0),
                    LoadLog::TYPE_LOG_TRUNCATED, LoadLog::PHASE_NONE);
}

TEST(LoadLogTest, Append) {
  scoped_refptr<LoadLog> log1(new LoadLog);
  scoped_refptr<LoadLog> log2(new LoadLog);

  log1->Add(MakeTime(0), LoadLog::TYPE_HOST_RESOLVER_IMPL,
            LoadLog::PHASE_BEGIN);

  log2->Add(MakeTime(3), LoadLog::TYPE_CANCELLED, LoadLog::PHASE_NONE);
  log2->Add(MakeTime(9), LoadLog::TYPE_HOST_RESOLVER_IMPL, LoadLog::PHASE_END);

  log1->Append(log2);

  // Add something else to log2 (should NOT be reflected in log1).
  log2->Add(MakeTime(19), LoadLog::TYPE_CANCELLED, LoadLog::PHASE_NONE);

  EXPECT_EQ(3u, log1->events().size());

  ExpectLogContains(log1, 0, MakeTime(0), LoadLog::TYPE_HOST_RESOLVER_IMPL,
                    LoadLog::PHASE_BEGIN);

  ExpectLogContains(log1, 1, MakeTime(3), LoadLog::TYPE_CANCELLED,
                    LoadLog::PHASE_NONE);

  ExpectLogContains(log1, 2, MakeTime(9), LoadLog::TYPE_HOST_RESOLVER_IMPL,
                    LoadLog::PHASE_END);
}

TEST(LoadLogTest, AppendWithTruncation) {
  // Append() should also respect the maximum number of entries bound.
  // (This is basically the same test as LoadLogTest.Truncation).

  // Create two load logs, which are 2/3 capcity.
  scoped_refptr<LoadLog> log1(new LoadLog);
  scoped_refptr<LoadLog> log2(new LoadLog);
  for (size_t i = 0; i < 2 * LoadLog::kMaxNumEntries / 3; ++i) {
    log1->Add(MakeTime(1), LoadLog::TYPE_CANCELLED, LoadLog::PHASE_NONE);
    log2->Add(MakeTime(2), LoadLog::TYPE_CANCELLED, LoadLog::PHASE_NONE);
  }

  // Append log2 to log1.
  log1->Append(log2);

  EXPECT_EQ(LoadLog::kMaxNumEntries, log1->events().size());

  // We terminated with a "truncation" event.
  ExpectLogContains(log1, LoadLog::kMaxNumEntries - 1, MakeTime(2),
                    LoadLog::TYPE_LOG_TRUNCATED, LoadLog::PHASE_NONE);
}

TEST(LoadLogTest, EventTypeToString) {
  EXPECT_STREQ("HOST_RESOLVER_IMPL",
               LoadLog::EventTypeToString(LoadLog::TYPE_HOST_RESOLVER_IMPL));
  EXPECT_STREQ("HOST_RESOLVER_IMPL_OBSERVER_ONSTART",
               LoadLog::EventTypeToString(
                  LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART));
}

}  // namespace
}  // namespace net
