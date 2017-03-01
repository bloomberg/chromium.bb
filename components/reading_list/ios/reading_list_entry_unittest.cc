// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/ios/reading_list_entry.h"

#include "base/memory/ptr_util.h"
#include "base/test/ios/wait_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "components/reading_list/ios/proto/reading_list.pb.h"
#include "components/sync/protocol/reading_list_specifics.pb.h"
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

TEST(ReadingListEntry, ReadState) {
  ReadingListEntry e(GURL("http://example.com"), "bar");
  EXPECT_FALSE(e.HasBeenSeen());
  EXPECT_FALSE(e.IsRead());
  e.SetRead(false);
  EXPECT_TRUE(e.HasBeenSeen());
  EXPECT_FALSE(e.IsRead());
  e.SetRead(true);
  EXPECT_TRUE(e.HasBeenSeen());
  EXPECT_TRUE(e.IsRead());
}

TEST(ReadingListEntry, UpdateTitle) {
  ReadingListEntry e(GURL("http://example.com"), "bar");
  ASSERT_EQ("bar", e.Title());
  ASSERT_EQ(e.CreationTime(), e.UpdateTitleTime());

  base::test::ios::SpinRunLoopWithMinDelay(
      base::TimeDelta::FromMilliseconds(5));
  e.SetTitle("foo");
  EXPECT_GT(e.UpdateTitleTime(), e.CreationTime());
  EXPECT_EQ("foo", e.Title());
}

TEST(ReadingListEntry, DistilledInfo) {
  ReadingListEntry e(GURL("http://example.com"), "bar");

  EXPECT_TRUE(e.DistilledPath().empty());

  const base::FilePath distilled_path("distilled/page.html");
  const GURL distilled_url("http://example.com/distilled");
  int64_t size = 50;
  int64_t time = 100;
  e.SetDistilledInfo(distilled_path, distilled_url, size, time);
  EXPECT_EQ(distilled_path, e.DistilledPath());
  EXPECT_EQ(distilled_url, e.DistilledURL());
  EXPECT_EQ(size, e.DistillationSize());
  EXPECT_EQ(e.DistillationTime(), time);
}

TEST(ReadingListEntry, DistilledState) {
  ReadingListEntry e(GURL("http://example.com"), "bar");

  EXPECT_EQ(ReadingListEntry::WAITING, e.DistilledState());

  e.SetDistilledState(ReadingListEntry::ERROR);
  EXPECT_EQ(ReadingListEntry::ERROR, e.DistilledState());

  const base::FilePath distilled_path("distilled/page.html");
  const GURL distilled_url("http://example.com/distilled");
  e.SetDistilledInfo(distilled_path, distilled_url, 50, 100);
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
  const base::FilePath distilled_path("distilled/page.html");
  const GURL distilled_url("http://example.com/distilled");
  e.SetDistilledInfo(distilled_path, distilled_url, 50, 100);

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

// Tests that the reading list entry is correctly encoded to
// sync_pb::ReadingListSpecifics.
TEST(ReadingListEntry, AsReadingListSpecifics) {
  ReadingListEntry entry(GURL("http://example.com/"), "bar");
  int64_t creation_time_us = entry.UpdateTime();

  std::unique_ptr<sync_pb::ReadingListSpecifics> pb_entry(
      entry.AsReadingListSpecifics());
  EXPECT_EQ(pb_entry->entry_id(), "http://example.com/");
  EXPECT_EQ(pb_entry->url(), "http://example.com/");
  EXPECT_EQ(pb_entry->title(), "bar");
  EXPECT_EQ(pb_entry->creation_time_us(), creation_time_us);
  EXPECT_EQ(pb_entry->update_time_us(), entry.UpdateTime());
  EXPECT_EQ(pb_entry->status(), sync_pb::ReadingListSpecifics::UNSEEN);

  entry.SetRead(true);
  EXPECT_NE(entry.UpdateTime(), creation_time_us);
  std::unique_ptr<sync_pb::ReadingListSpecifics> updated_pb_entry(
      entry.AsReadingListSpecifics());
  EXPECT_EQ(updated_pb_entry->creation_time_us(), creation_time_us);
  EXPECT_EQ(updated_pb_entry->update_time_us(), entry.UpdateTime());
  EXPECT_EQ(updated_pb_entry->status(), sync_pb::ReadingListSpecifics::READ);
}

// Tests that the reading list entry is correctly parsed from
// sync_pb::ReadingListSpecifics.
TEST(ReadingListEntry, FromReadingListSpecifics) {
  std::unique_ptr<sync_pb::ReadingListSpecifics> pb_entry =
      base::MakeUnique<sync_pb::ReadingListSpecifics>();
  pb_entry->set_entry_id("http://example.com/");
  pb_entry->set_url("http://example.com/");
  pb_entry->set_title("title");
  pb_entry->set_creation_time_us(1);
  pb_entry->set_update_time_us(2);
  pb_entry->set_status(sync_pb::ReadingListSpecifics::UNREAD);

  std::unique_ptr<ReadingListEntry> entry(
      ReadingListEntry::FromReadingListSpecifics(*pb_entry));
  EXPECT_EQ(entry->URL().spec(), "http://example.com/");
  EXPECT_EQ(entry->Title(), "title");
  EXPECT_EQ(entry->UpdateTime(), 2);
  EXPECT_EQ(entry->FailedDownloadCounter(), 0);
}

// Tests that the reading list entry is correctly encoded to
// reading_list::ReadingListLocal.
TEST(ReadingListEntry, AsReadingListLocal) {
  ReadingListEntry entry(GURL("http://example.com/"), "bar");
  int64_t creation_time_us = entry.UpdateTime();

  std::unique_ptr<reading_list::ReadingListLocal> pb_entry(
      entry.AsReadingListLocal());
  EXPECT_EQ(pb_entry->entry_id(), "http://example.com/");
  EXPECT_EQ(pb_entry->url(), "http://example.com/");
  EXPECT_EQ(pb_entry->title(), "bar");
  EXPECT_EQ(pb_entry->creation_time_us(), creation_time_us);
  EXPECT_EQ(pb_entry->update_time_us(), entry.UpdateTime());
  EXPECT_EQ(pb_entry->status(), reading_list::ReadingListLocal::UNSEEN);
  EXPECT_EQ(pb_entry->distillation_state(),
            reading_list::ReadingListLocal::WAITING);
  EXPECT_EQ(pb_entry->distilled_path(), "");
  EXPECT_EQ(pb_entry->failed_download_counter(), 0);
  EXPECT_NE(pb_entry->backoff(), "");

  entry.SetDistilledState(ReadingListEntry::WILL_RETRY);
  std::unique_ptr<reading_list::ReadingListLocal> will_retry_pb_entry(
      entry.AsReadingListLocal());
  EXPECT_EQ(will_retry_pb_entry->distillation_state(),
            reading_list::ReadingListLocal::WILL_RETRY);
  EXPECT_EQ(will_retry_pb_entry->failed_download_counter(), 1);

  const base::FilePath distilled_path("distilled/page.html");
  const GURL distilled_url("http://example.com/distilled");
  int64_t size = 50;
  entry.SetDistilledInfo(distilled_path, distilled_url, size, 100);

  entry.SetRead(true);
  entry.MarkEntryUpdated();
  EXPECT_NE(entry.UpdateTime(), creation_time_us);
  std::unique_ptr<reading_list::ReadingListLocal> distilled_pb_entry(
      entry.AsReadingListLocal());
  EXPECT_EQ(distilled_pb_entry->creation_time_us(), creation_time_us);
  EXPECT_EQ(distilled_pb_entry->update_time_us(), entry.UpdateTime());
  EXPECT_NE(distilled_pb_entry->backoff(), "");
  EXPECT_EQ(distilled_pb_entry->status(), reading_list::ReadingListLocal::READ);
  EXPECT_EQ(distilled_pb_entry->distillation_state(),
            reading_list::ReadingListLocal::PROCESSED);
  EXPECT_EQ(distilled_pb_entry->distilled_path(), "distilled/page.html");
  EXPECT_EQ(distilled_pb_entry->failed_download_counter(), 0);
  EXPECT_EQ(distilled_pb_entry->distillation_time_us(),
            entry.DistillationTime());
  EXPECT_EQ(distilled_pb_entry->distillation_size(), entry.DistillationSize());
}

// Tests that the reading list entry is correctly parsed from
// sync_pb::ReadingListLocal.
TEST(ReadingListEntry, FromReadingListLocal) {
  ReadingListEntry entry(GURL("http://example.com/"), "title");
  entry.SetDistilledState(ReadingListEntry::ERROR);

  std::unique_ptr<reading_list::ReadingListLocal> pb_entry(
      entry.AsReadingListLocal());
  int64_t now = 12345;

  pb_entry->set_entry_id("http://example.com/");
  pb_entry->set_url("http://example.com/");
  pb_entry->set_title("title");
  pb_entry->set_creation_time_us(1);
  pb_entry->set_update_time_us(2);
  pb_entry->set_status(reading_list::ReadingListLocal::UNREAD);
  pb_entry->set_distillation_state(reading_list::ReadingListLocal::WAITING);
  pb_entry->set_failed_download_counter(2);
  pb_entry->set_distillation_time_us(now);
  pb_entry->set_distillation_size(50);

  std::unique_ptr<ReadingListEntry> waiting_entry(
      ReadingListEntry::FromReadingListLocal(*pb_entry));
  EXPECT_EQ(waiting_entry->URL().spec(), "http://example.com/");
  EXPECT_EQ(waiting_entry->Title(), "title");
  EXPECT_EQ(waiting_entry->UpdateTime(), 2);
  EXPECT_EQ(waiting_entry->FailedDownloadCounter(), 2);
  EXPECT_EQ(waiting_entry->DistilledState(), ReadingListEntry::WAITING);
  EXPECT_EQ(waiting_entry->DistilledPath(), base::FilePath());
  EXPECT_EQ(waiting_entry->DistillationSize(), 50);
  EXPECT_EQ(waiting_entry->DistillationTime(), now);
  double fuzzing = ReadingListEntry::kBackoffPolicy.jitter_factor;
  int nextTry = waiting_entry->TimeUntilNextTry().InMinutes();
  EXPECT_NEAR(kFirstBackoff, nextTry, kFirstBackoff * fuzzing);
}

// Tests the merging of two ReadingListEntry.
// Additional merging tests are done in
// ReadingListStoreTest.CompareEntriesForSync
TEST(ReadingListEntry, MergeWithEntry) {
  ReadingListEntry local_entry(GURL("http://example.com/"), "title");
  local_entry.SetDistilledState(ReadingListEntry::ERROR);
  int64_t local_update_time_us = local_entry.UpdateTime();

  ReadingListEntry sync_entry(GURL("http://example.com/"), "title2");
  sync_entry.SetDistilledState(ReadingListEntry::ERROR);
  int64_t sync_update_time_us = sync_entry.UpdateTime();
  EXPECT_NE(local_update_time_us, sync_update_time_us);
  local_entry.MergeWithEntry(sync_entry);
  EXPECT_EQ(local_entry.URL().spec(), "http://example.com/");
  EXPECT_EQ(local_entry.Title(), "title2");
  EXPECT_FALSE(local_entry.HasBeenSeen());
  EXPECT_EQ(local_entry.UpdateTime(), sync_update_time_us);
  EXPECT_EQ(local_entry.FailedDownloadCounter(), 1);
  EXPECT_EQ(local_entry.DistilledState(), ReadingListEntry::ERROR);
  double fuzzing = ReadingListEntry::kBackoffPolicy.jitter_factor;
  int nextTry = local_entry.TimeUntilNextTry().InMinutes();
  EXPECT_NEAR(kFirstBackoff, nextTry, kFirstBackoff * fuzzing);
}
