// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/load_log_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(LoadLogTest, Nullable) {
  // Make sure that the static methods can be called with NULL (no-op).
  // (Should not crash).
  LoadLog::BeginEvent(NULL, LoadLog::TYPE_HOST_RESOLVER_IMPL);
  LoadLog::AddEvent(NULL, LoadLog::TYPE_HOST_RESOLVER_IMPL);
  LoadLog::EndEvent(NULL, LoadLog::TYPE_HOST_RESOLVER_IMPL);
}

TEST(LoadLogTest, Basic) {
  scoped_refptr<LoadLog> log(new LoadLog(10));

  // Logs start off empty.
  EXPECT_EQ(0u, log->entries().size());
  EXPECT_EQ(0u, log->num_entries_truncated());

  // Add 3 entries.

  log->Add(LoadLog::Entry(MakeTime(0),
                          LoadLog::Event(LoadLog::TYPE_HOST_RESOLVER_IMPL,
                                         LoadLog::PHASE_BEGIN)));
  log->Add(LoadLog::Entry(MakeTime(2),
                          LoadLog::Event(LoadLog::TYPE_CANCELLED,
                                         LoadLog::PHASE_NONE)));
  log->Add(
      LoadLog::Entry(
          MakeTime(11),
          LoadLog::Event(LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART,
                         LoadLog::PHASE_END)));

  EXPECT_EQ(3u, log->entries().size());
  EXPECT_EQ(0u, log->num_entries_truncated());

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
  size_t kMaxNumEntries = 10;
  scoped_refptr<LoadLog> log(new LoadLog(kMaxNumEntries));

  // Max it out.
  for (size_t i = 0; i < kMaxNumEntries; ++i) {
    log->Add(LoadLog::Entry(MakeTime(i),
                            LoadLog::Event(LoadLog::TYPE_CANCELLED,
                                           LoadLog::PHASE_NONE)));
  }

  EXPECT_EQ(kMaxNumEntries, log->entries().size());
  EXPECT_EQ(0u, log->num_entries_truncated());

  // Check the last entry.
  ExpectLogContains(log, 9, MakeTime(9),
                    LoadLog::TYPE_CANCELLED,
                    LoadLog::PHASE_NONE);

  // Add three entries while maxed out (will cause truncation)
  log->Add(LoadLog::Entry(MakeTime(0),
                          LoadLog::Event(LoadLog::TYPE_CANCELLED,
                                         LoadLog::PHASE_NONE)));
  log->Add(LoadLog::Entry(MakeTime(1),
                          LoadLog::Event(LoadLog::TYPE_CANCELLED,
                                         LoadLog::PHASE_NONE)));
  log->Add(LoadLog::Entry(MakeTime(2),
                          LoadLog::Event(LoadLog::TYPE_CANCELLED,
                                         LoadLog::PHASE_NONE)));

  EXPECT_EQ(kMaxNumEntries, log->entries().size());
  EXPECT_EQ(3u, log->num_entries_truncated());

  // Check the last entry -- it should be the final entry we added.
  ExpectLogContains(log, 9, MakeTime(2),
                    LoadLog::TYPE_CANCELLED,
                    LoadLog::PHASE_NONE);
}

TEST(LoadLogTest, Append) {
  scoped_refptr<LoadLog> log1(new LoadLog(10));
  scoped_refptr<LoadLog> log2(new LoadLog(10));

  log1->Add(LoadLog::Entry(MakeTime(0),
                           LoadLog::Event(LoadLog::TYPE_HOST_RESOLVER_IMPL,
                                          LoadLog::PHASE_BEGIN)));

  log2->Add(LoadLog::Entry(MakeTime(3),
                           LoadLog::Event(LoadLog::TYPE_CANCELLED,
                                          LoadLog::PHASE_NONE)));
  log2->Add(LoadLog::Entry(MakeTime(9),
                           LoadLog::Event(LoadLog::TYPE_HOST_RESOLVER_IMPL,
                                          LoadLog::PHASE_END)));

  log1->Append(log2);

  // Add something else to log2 (should NOT be reflected in log1).
  log2->Add(LoadLog::Entry(MakeTime(19),
                           LoadLog::Event(LoadLog::TYPE_CANCELLED,
                                          LoadLog::PHASE_NONE)));

  EXPECT_EQ(3u, log1->entries().size());
  EXPECT_EQ(0u, log1->num_entries_truncated());

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

  // Create two load logs, which both have 6 out of 10 entries filled.
  size_t kMaxNumEntries = 10;
  scoped_refptr<LoadLog> log1(new LoadLog(kMaxNumEntries));
  scoped_refptr<LoadLog> log2(new LoadLog(kMaxNumEntries));
  for (size_t i = 0; i < 6; ++i) {
    log1->Add(LoadLog::Entry(MakeTime(i),
                             LoadLog::Event(LoadLog::TYPE_CANCELLED,
                                            LoadLog::PHASE_NONE)));
    log2->Add(LoadLog::Entry(MakeTime(2 * i),
                             LoadLog::Event(LoadLog::TYPE_CANCELLED,
                                            LoadLog::PHASE_NONE)));
  }

  // Append log2 to log1.
  log1->Append(log2);

  // The combined log totalled 12 entries, so 2 must have been dropped.
  EXPECT_EQ(10u, log1->entries().size());
  EXPECT_EQ(2u, log1->num_entries_truncated());

  // Combined log should end with the final entry of log2.
  ExpectLogContains(log1, 9, MakeTime(10), LoadLog::TYPE_CANCELLED,
                    LoadLog::PHASE_NONE);
}

TEST(LoadLogTest, EventTypeToString) {
  EXPECT_STREQ("HOST_RESOLVER_IMPL",
               LoadLog::EventTypeToString(LoadLog::TYPE_HOST_RESOLVER_IMPL));
  EXPECT_STREQ("HOST_RESOLVER_IMPL_OBSERVER_ONSTART",
               LoadLog::EventTypeToString(
                  LoadLog::TYPE_HOST_RESOLVER_IMPL_OBSERVER_ONSTART));
}

TEST(LoadLogTest, String) {
  scoped_refptr<LoadLog> log(new LoadLog(10));

  // Make sure that AddStringLiteral() adds a literal and not a std::string.
  // (in case there is any funny-business with implicit conversions).
  LoadLog::AddStringLiteral(log, "This is a literal");
  log->Add(LoadLog::Entry(MakeTime(0), "Another literal"));
  log->Add(LoadLog::Entry(MakeTime(0), std::string("Now a std::string")));

  ASSERT_EQ(3u, log->entries().size());
  EXPECT_EQ(LoadLog::Entry::TYPE_STRING_LITERAL, log->entries()[0].type);
  EXPECT_EQ(LoadLog::Entry::TYPE_STRING_LITERAL, log->entries()[1].type);
  EXPECT_EQ(LoadLog::Entry::TYPE_STRING, log->entries()[2].type);
}

}  // namespace
}  // namespace net
