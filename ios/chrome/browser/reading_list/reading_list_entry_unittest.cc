// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_entry.h"

#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ReadingListEntry, CompareIgnoreTitle) {
  const ReadingListEntry e1(GURL("http://example.com"), "bar");
  const ReadingListEntry e2(GURL("http://example.com"), "foo");

  EXPECT_EQ(e1, e2);
}

TEST(ReadingListEntry, CompareFailureIgnoreTitle) {
  const ReadingListEntry e1(GURL("http://example.com"), "bar");
  const ReadingListEntry e2(GURL("http://example.org"), "bar");

  EXPECT_FALSE(e1 == e2);
}

TEST(ReadingListEntry, MovesAreEquals) {
  ReadingListEntry e1(GURL("http://example.com"), "bar");
  ReadingListEntry e2(GURL("http://example.com"), "bar");
  ASSERT_EQ(e1, e2);
  ASSERT_EQ(e1.Title(), e2.Title());

  ReadingListEntry e3(std::move(e1));

  EXPECT_EQ(e3, e2);
  EXPECT_EQ(e3.Title(), e2.Title());
}

TEST(ReadingListEntry, DistilledURL) {
  ReadingListEntry e(GURL("http://example.com"), "bar");

  EXPECT_FALSE(e.DistilledURL().is_valid());

  const GURL distilled_url("http://distilled.example.com");
  e.SetDistilledURL(distilled_url);
  EXPECT_EQ(distilled_url, e.DistilledURL());
}

TEST(ReadingListEntry, DistilledState) {
  ReadingListEntry e(GURL("http://example.com"), "bar");

  EXPECT_EQ(ReadingListEntry::WAITING, e.DistilledState());

  e.SetDistilledState(ReadingListEntry::ERROR);
  EXPECT_EQ(ReadingListEntry::ERROR, e.DistilledState());

  const GURL distilled_url("http://distilled.example.com");
  e.SetDistilledURL(distilled_url);
  EXPECT_EQ(ReadingListEntry::PROCESSED, e.DistilledState());
}

// Tests that the the time until next try increase exponentially when the state
// changes from non-error to error.
TEST(ReadingListEntry, TimeUntilNextTry) {
  base::SimpleTestTickClock clock;
  std::unique_ptr<net::BackoffEntry> backoff =
      base::MakeUnique<net::BackoffEntry>(&ReadingListEntry::kBackoffPolicy,
                                          &clock);

  ReadingListEntry e(GURL("http://example.com"), "bar", std::move(backoff));

  ASSERT_EQ(0, e.TimeUntilNextTry().InSeconds());

  e.SetDistilledState(ReadingListEntry::ERROR);
  EXPECT_EQ(1, e.TimeUntilNextTry().InSeconds());
  e.SetDistilledState(ReadingListEntry::WILL_RETRY);
  EXPECT_EQ(1, e.TimeUntilNextTry().InSeconds());

  e.SetDistilledState(ReadingListEntry::PROCESSING);
  EXPECT_EQ(1, e.TimeUntilNextTry().InSeconds());

  e.SetDistilledState(ReadingListEntry::WILL_RETRY);
  EXPECT_EQ(2, e.TimeUntilNextTry().InSeconds());
  e.SetDistilledState(ReadingListEntry::ERROR);
  EXPECT_EQ(2, e.TimeUntilNextTry().InSeconds());

  e.SetDistilledState(ReadingListEntry::PROCESSING);
  EXPECT_EQ(2, e.TimeUntilNextTry().InSeconds());

  e.SetDistilledState(ReadingListEntry::WILL_RETRY);
  EXPECT_EQ(4, e.TimeUntilNextTry().InSeconds());
}

// Tests that if the time until next try is in the past, 0 is returned.
TEST(ReadingListEntry, TimeUntilNextTryInThePast) {
  // Setup.
  base::SimpleTestTickClock clock;
  std::unique_ptr<net::BackoffEntry> backoff =
      base::MakeUnique<net::BackoffEntry>(&ReadingListEntry::kBackoffPolicy,
                                          &clock);
  ReadingListEntry e(GURL("http://example.com"), "bar", std::move(backoff));

  e.SetDistilledState(ReadingListEntry::ERROR);
  ASSERT_EQ(1, e.TimeUntilNextTry().InSeconds());

  // Action.
  clock.Advance(base::TimeDelta::FromSeconds(2));

  // Test.
  EXPECT_EQ(0, e.TimeUntilNextTry().InMilliseconds());
}

// Tests that if the time until next try is in the past, 0 is returned.
TEST(ReadingListEntry, ResetTimeUntilNextTry) {
  // Setup.
  base::SimpleTestTickClock clock;
  std::unique_ptr<net::BackoffEntry> backoff =
      base::MakeUnique<net::BackoffEntry>(&ReadingListEntry::kBackoffPolicy,
                                          &clock);
  ReadingListEntry e(GURL("http://example.com"), "bar", std::move(backoff));

  e.SetDistilledState(ReadingListEntry::ERROR);
  e.SetDistilledState(ReadingListEntry::PROCESSING);
  e.SetDistilledState(ReadingListEntry::ERROR);
  ASSERT_EQ(2, e.TimeUntilNextTry().InSeconds());

  // Action.
  e.SetDistilledURL(GURL("http://example.com"));

  // Test.
  EXPECT_EQ(0, e.TimeUntilNextTry().InSeconds());
  e.SetDistilledState(ReadingListEntry::ERROR);
  EXPECT_EQ(1, e.TimeUntilNextTry().InSeconds());
}

// Tests that the failed download counter is incremented when the state change
// from non-error to error.
TEST(ReadingListEntry, FailedDownloadCounter) {
  ReadingListEntry e(GURL("http://example.com"), "bar");

  ASSERT_EQ(0, e.FailedDownloadCounter());

  e.SetDistilledState(ReadingListEntry::ERROR);
  EXPECT_EQ(1, e.FailedDownloadCounter());
  e.SetDistilledState(ReadingListEntry::WILL_RETRY);
  EXPECT_EQ(1, e.FailedDownloadCounter());

  e.SetDistilledState(ReadingListEntry::PROCESSING);
  EXPECT_EQ(1, e.FailedDownloadCounter());

  e.SetDistilledState(ReadingListEntry::WILL_RETRY);
  EXPECT_EQ(2, e.FailedDownloadCounter());
  e.SetDistilledState(ReadingListEntry::ERROR);
  EXPECT_EQ(2, e.FailedDownloadCounter());
}
