// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_entry.h"

#include "base/memory/ptr_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const int kFirstBackoff = 10;
const int kSecondBackoff = 10;
const int kThirdBackoff = 60;
const int kFourthBackoff = 120;
const int kFifthBackoff = 120;
}  // namespace

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

TEST(ReadingListEntry, DistilledPathAndURL) {
  ReadingListEntry e(GURL("http://example.com"), "bar");

  EXPECT_FALSE(e.DistilledURL().is_valid());

  const base::FilePath distilled_path("distilled/page.html");
  e.SetDistilledPath(distilled_path);
  EXPECT_EQ(distilled_path, e.DistilledPath());
  EXPECT_EQ(GURL("chrome://offline/distilled/page.html"), e.DistilledURL());
}

TEST(ReadingListEntry, DistilledState) {
  ReadingListEntry e(GURL("http://example.com"), "bar");

  EXPECT_EQ(ReadingListEntry::WAITING, e.DistilledState());

  e.SetDistilledState(ReadingListEntry::ERROR);
  EXPECT_EQ(ReadingListEntry::ERROR, e.DistilledState());

  const base::FilePath distilled_path("distilled/page.html");
  e.SetDistilledPath(distilled_path);
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

  double fuzzing = ReadingListEntry::kBackoffPolicy.jitter_factor;

  ASSERT_EQ(0, e.TimeUntilNextTry().InSeconds());

  // First error.
  e.SetDistilledState(ReadingListEntry::ERROR);
  int nextTry = e.TimeUntilNextTry().InMinutes();
  EXPECT_NEAR(kFirstBackoff, nextTry, kFirstBackoff * fuzzing);
  e.SetDistilledState(ReadingListEntry::WILL_RETRY);
  EXPECT_EQ(nextTry, e.TimeUntilNextTry().InMinutes());

  e.SetDistilledState(ReadingListEntry::PROCESSING);
  EXPECT_EQ(nextTry, e.TimeUntilNextTry().InMinutes());

  // Second error.
  e.SetDistilledState(ReadingListEntry::WILL_RETRY);
  nextTry = e.TimeUntilNextTry().InMinutes();
  EXPECT_NEAR(kSecondBackoff, nextTry, kSecondBackoff * fuzzing);
  e.SetDistilledState(ReadingListEntry::ERROR);
  EXPECT_EQ(nextTry, e.TimeUntilNextTry().InMinutes());

  e.SetDistilledState(ReadingListEntry::PROCESSING);
  EXPECT_EQ(nextTry, e.TimeUntilNextTry().InMinutes());

  // Third error.
  e.SetDistilledState(ReadingListEntry::WILL_RETRY);
  EXPECT_NEAR(kThirdBackoff, e.TimeUntilNextTry().InMinutes(),
              kThirdBackoff * fuzzing);

  // Fourth error.
  e.SetDistilledState(ReadingListEntry::PROCESSING);
  e.SetDistilledState(ReadingListEntry::ERROR);
  EXPECT_NEAR(kFourthBackoff, e.TimeUntilNextTry().InMinutes(),
              kFourthBackoff * fuzzing);

  // Fifth error.
  e.SetDistilledState(ReadingListEntry::PROCESSING);
  e.SetDistilledState(ReadingListEntry::ERROR);
  EXPECT_NEAR(kFifthBackoff, e.TimeUntilNextTry().InMinutes(),
              kFifthBackoff * fuzzing);
}

// Tests that if the time until next try is in the past, 0 is returned.
TEST(ReadingListEntry, TimeUntilNextTryInThePast) {
  // Setup.
  base::SimpleTestTickClock clock;
  std::unique_ptr<net::BackoffEntry> backoff =
      base::MakeUnique<net::BackoffEntry>(&ReadingListEntry::kBackoffPolicy,
                                          &clock);
  ReadingListEntry e(GURL("http://example.com"), "bar", std::move(backoff));
  double fuzzing = ReadingListEntry::kBackoffPolicy.jitter_factor;

  e.SetDistilledState(ReadingListEntry::ERROR);
  ASSERT_NEAR(kFirstBackoff, e.TimeUntilNextTry().InMinutes(),
              kFirstBackoff * fuzzing);

  // Action.
  clock.Advance(base::TimeDelta::FromMinutes(kFirstBackoff * 2));

  // Test.
  EXPECT_EQ(0, e.TimeUntilNextTry().InMilliseconds());
}

// Tests that if the entry gets a distilled URL, 0 is returned.
TEST(ReadingListEntry, ResetTimeUntilNextTry) {
  // Setup.
  base::SimpleTestTickClock clock;
  std::unique_ptr<net::BackoffEntry> backoff =
      base::MakeUnique<net::BackoffEntry>(&ReadingListEntry::kBackoffPolicy,
                                          &clock);
  ReadingListEntry e(GURL("http://example.com"), "bar", std::move(backoff));
  double fuzzing = ReadingListEntry::kBackoffPolicy.jitter_factor;

  e.SetDistilledState(ReadingListEntry::ERROR);
  ASSERT_NEAR(kFirstBackoff, e.TimeUntilNextTry().InMinutes(),
              kFirstBackoff * fuzzing);

  // Action.
  e.SetDistilledPath(base::FilePath("distilled/page.html"));

  // Test.
  EXPECT_EQ(0, e.TimeUntilNextTry().InSeconds());
  e.SetDistilledState(ReadingListEntry::ERROR);
  ASSERT_NEAR(kFirstBackoff, e.TimeUntilNextTry().InMinutes(),
              kFirstBackoff * fuzzing);
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
