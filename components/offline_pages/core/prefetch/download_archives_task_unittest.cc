// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/download_archives_task.h"

#include <algorithm>
#include <set>

#include "base/guid.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_utils.h"
#include "components/offline_pages/core/prefetch/task_test_base.h"
#include "components/offline_pages/core/prefetch/test_prefetch_downloader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const PrefetchItem* FindPrefetchItemByOfflineId(
    const std::set<PrefetchItem>& items,
    int64_t offline_id) {
  auto found_it = std::find_if(items.begin(), items.end(),
                               [&offline_id](const PrefetchItem& i) -> bool {
                                 return i.offline_id == offline_id;
                               });
  if (found_it != items.end())
    return &(*found_it);
  return nullptr;
}

class DownloadArchivesTaskTest : public TaskTestBase {
 public:
  TestPrefetchDownloader* prefetch_downloader() {
    return &test_prefetch_downloader_;
  }

  int64_t InsertDummyItem();
  void InsertDummyItemInState(PrefetchItemState state);
  int64_t InsertItemToDownload();

 private:
  TestPrefetchDownloader test_prefetch_downloader_;
};

int64_t DownloadArchivesTaskTest::InsertDummyItem() {
  PrefetchItem item = item_generator()->CreateItem(
      PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
  store_util()->InsertPrefetchItem(item);
  return item.offline_id;
}

void DownloadArchivesTaskTest::InsertDummyItemInState(PrefetchItemState state) {
  store_util()->InsertPrefetchItem(item_generator()->CreateItem(state));
}

int64_t DownloadArchivesTaskTest::InsertItemToDownload() {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::RECEIVED_BUNDLE);
  store_util()->InsertPrefetchItem(item);
  return item.offline_id;
}

TEST_F(DownloadArchivesTaskTest, NoArchivesToDownload) {
  InsertDummyItemInState(PrefetchItemState::NEW_REQUEST);
  InsertDummyItemInState(PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE);
  InsertDummyItemInState(PrefetchItemState::AWAITING_GCM);
  InsertDummyItemInState(PrefetchItemState::RECEIVED_GCM);
  InsertDummyItemInState(PrefetchItemState::SENT_GET_OPERATION);
  InsertDummyItemInState(PrefetchItemState::DOWNLOADING);
  InsertDummyItemInState(PrefetchItemState::DOWNLOADED);
  InsertDummyItemInState(PrefetchItemState::IMPORTING);
  InsertDummyItemInState(PrefetchItemState::FINISHED);
  InsertDummyItemInState(PrefetchItemState::ZOMBIE);

  std::set<PrefetchItem> items_before_run;
  EXPECT_EQ(10U, store_util()->GetAllItems(&items_before_run));

  DownloadArchivesTask task(store(), prefetch_downloader());
  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();

  std::set<PrefetchItem> items_after_run;
  EXPECT_EQ(10U, store_util()->GetAllItems(&items_after_run));

  EXPECT_EQ(items_before_run, items_after_run);
}

TEST_F(DownloadArchivesTaskTest, SingleArchiveToDownload) {
  int64_t dummy_item_id = InsertDummyItem();
  int64_t download_item_id = InsertItemToDownload();

  std::set<PrefetchItem> items_before_run;
  EXPECT_EQ(2U, store_util()->GetAllItems(&items_before_run));

  DownloadArchivesTask task(store(), prefetch_downloader());
  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();

  std::set<PrefetchItem> items_after_run;
  EXPECT_EQ(2U, store_util()->GetAllItems(&items_after_run));

  const PrefetchItem* dummy_item_before =
      FindPrefetchItemByOfflineId(items_before_run, dummy_item_id);
  const PrefetchItem* dummy_item_after =
      FindPrefetchItemByOfflineId(items_after_run, dummy_item_id);
  ASSERT_TRUE(dummy_item_before);
  ASSERT_TRUE(dummy_item_after);
  EXPECT_EQ(*dummy_item_before, *dummy_item_after);

  const PrefetchItem* download_item_before =
      FindPrefetchItemByOfflineId(items_before_run, download_item_id);
  EXPECT_EQ(0, download_item_before->download_initiation_attempts);

  const PrefetchItem* download_item =
      FindPrefetchItemByOfflineId(items_after_run, download_item_id);
  ASSERT_TRUE(download_item);
  EXPECT_EQ(PrefetchItemState::DOWNLOADING, download_item->state);
  EXPECT_EQ(1, download_item->download_initiation_attempts);
  // These times are created using base::Time::Now() in short distance from each
  // other, therefore putting *_LE was considered too.
  EXPECT_LT(download_item_before->freshness_time,
            download_item->freshness_time);

  std::map<std::string, std::string> requested_downloads =
      prefetch_downloader()->requested_downloads();
  auto it = requested_downloads.find(download_item->guid);
  ASSERT_TRUE(it != requested_downloads.end());
  EXPECT_EQ(it->second, download_item->archive_body_name);
}

TEST_F(DownloadArchivesTaskTest, MultipleArchivesToDownload) {
  int64_t dummy_item_id = InsertDummyItem();
  int64_t download_item_id_1 = InsertItemToDownload();
  int64_t download_item_id_2 = InsertItemToDownload();

  std::set<PrefetchItem> items_before_run;
  EXPECT_EQ(3U, store_util()->GetAllItems(&items_before_run));

  DownloadArchivesTask task(store(), prefetch_downloader());
  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();

  std::set<PrefetchItem> items_after_run;
  EXPECT_EQ(3U, store_util()->GetAllItems(&items_after_run));

  const PrefetchItem* dummy_item_before =
      FindPrefetchItemByOfflineId(items_before_run, dummy_item_id);
  const PrefetchItem* dummy_item_after =
      FindPrefetchItemByOfflineId(items_after_run, dummy_item_id);
  ASSERT_TRUE(dummy_item_before);
  ASSERT_TRUE(dummy_item_after);
  EXPECT_EQ(*dummy_item_before, *dummy_item_after);

  const PrefetchItem* download_item_1 =
      FindPrefetchItemByOfflineId(items_after_run, download_item_id_1);
  ASSERT_TRUE(download_item_1);
  EXPECT_EQ(PrefetchItemState::DOWNLOADING, download_item_1->state);

  const PrefetchItem* download_item_2 =
      FindPrefetchItemByOfflineId(items_after_run, download_item_id_2);
  ASSERT_TRUE(download_item_2);
  EXPECT_EQ(PrefetchItemState::DOWNLOADING, download_item_2->state);

  std::map<std::string, std::string> requested_downloads =
      prefetch_downloader()->requested_downloads();
  EXPECT_EQ(2U, requested_downloads.size());

  auto it = requested_downloads.find(download_item_1->guid);
  ASSERT_TRUE(it != requested_downloads.end());
  EXPECT_EQ(it->second, download_item_1->archive_body_name);

  it = requested_downloads.find(download_item_2->guid);
  ASSERT_TRUE(it != requested_downloads.end());
  EXPECT_EQ(it->second, download_item_2->archive_body_name);
}

TEST_F(DownloadArchivesTaskTest, SingleArchiveSecondAttempt) {
  PrefetchItem item =
      item_generator()->CreateItem(PrefetchItemState::RECEIVED_BUNDLE);
  item.download_initiation_attempts = 1;
  item.freshness_time = base::Time::Now();
  item.guid = base::GenerateGUID();
  store_util()->InsertPrefetchItem(item);

  std::set<PrefetchItem> items_before_run;
  EXPECT_EQ(1U, store_util()->GetAllItems(&items_before_run));

  DownloadArchivesTask task(store(), prefetch_downloader());
  ExpectTaskCompletes(&task);
  task.Run();
  RunUntilIdle();

  std::set<PrefetchItem> items_after_run;
  EXPECT_EQ(1U, store_util()->GetAllItems(&items_after_run));

  const PrefetchItem* download_item =
      FindPrefetchItemByOfflineId(items_after_run, item.offline_id);
  ASSERT_TRUE(download_item);
  EXPECT_EQ(PrefetchItemState::DOWNLOADING, download_item->state);
  EXPECT_EQ(2, download_item->download_initiation_attempts);
  EXPECT_EQ(item.archive_body_name, download_item->archive_body_name);
  // GUID expected to change between download attempts.
  EXPECT_NE(item.guid, download_item->guid);
  // Freshness time not expected to change after first attempt.
  EXPECT_EQ(item.freshness_time, download_item->freshness_time);

  std::map<std::string, std::string> requested_downloads =
      prefetch_downloader()->requested_downloads();
  auto it = requested_downloads.find(download_item->guid);
  ASSERT_TRUE(it != requested_downloads.end());
  EXPECT_EQ(it->second, download_item->archive_body_name);
}

}  // namespace
}  // namespace offline_pages
