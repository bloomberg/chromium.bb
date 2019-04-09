// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/cleanup_thumbnails_task.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "components/offline_pages/core/model/get_thumbnail_task.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/model/store_visuals_task.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"

namespace offline_pages {
namespace {

class CleanupThumbnailsTaskTest : public ModelTaskTestBase {
 public:
  ~CleanupThumbnailsTaskTest() override {}

  std::unique_ptr<OfflinePageThumbnail> ReadThumbnail(int64_t offline_id) {
    std::unique_ptr<OfflinePageThumbnail> thumb;
    auto callback = [&](std::unique_ptr<OfflinePageThumbnail> result) {
      thumb = std::move(result);
    };
    RunTask(std::make_unique<GetThumbnailTask>(
        store(), offline_id, base::BindLambdaForTesting(callback)));
    return thumb;
  }

  OfflinePageThumbnail MustReadThumbnail(int64_t offline_id) {
    std::unique_ptr<OfflinePageThumbnail> thumb = ReadThumbnail(offline_id);
    CHECK(thumb);
    return *thumb;
  }

  void StoreVisuals(int64_t offline_id,
                    std::string thumbnail,
                    std::string favicon) {
    RunTask(StoreVisualsTask::MakeStoreThumbnailTask(
        store(), offline_id, thumbnail, base::DoNothing()));
    RunTask(StoreVisualsTask::MakeStoreFaviconTask(store(), offline_id, favicon,
                                                   base::DoNothing()));
  }
};

TEST_F(CleanupThumbnailsTaskTest, DbConnectionIsNull) {
  base::MockCallback<CleanupThumbnailsCallback> callback;
  EXPECT_CALL(callback, Run(false)).Times(1);
  store()->SetInitializationStatusForTesting(
      SqlStoreBase::InitializationStatus::kFailure, true);
  RunTask(std::make_unique<CleanupThumbnailsTask>(
      store(), store_utils::FromDatabaseTime(1000), callback.Get()));
}

TEST_F(CleanupThumbnailsTaskTest, CleanupNoThumbnails) {
  base::MockCallback<CleanupThumbnailsCallback> callback;
  EXPECT_CALL(callback, Run(true)).Times(1);

  base::HistogramTester histogram_tester;
  RunTask(std::make_unique<CleanupThumbnailsTask>(
      store(), store_utils::FromDatabaseTime(1000), callback.Get()));

  histogram_tester.ExpectUniqueSample("OfflinePages.CleanupThumbnails.Count", 0,
                                      1);
}

TEST_F(CleanupThumbnailsTaskTest, CleanupAllCombinations) {
  // Two conditions contribute to thumbnail cleanup: does a corresponding
  // OfflinePageItem exist, and is the thumbnail expired. All four combinations
  // of these states are tested.

  // Start slightly above base::Time() to avoid negative time below.
  TestScopedOfflineClock test_clock;
  test_clock.SetNow(base::Time() + base::TimeDelta::FromDays(1));

  // 1. Has item, not expired.
  OfflinePageItem item1 = generator()->CreateItem();
  store_test_util()->InsertItem(item1);

  OfflinePageThumbnail thumb1(item1.offline_id,
                              OfflineTimeNow() + kVisualsExpirationDelta,
                              "thumb1", "favicon1");
  StoreVisuals(thumb1.offline_id, thumb1.thumbnail, thumb1.favicon);

  // 2. Has item, expired.
  OfflinePageItem item2 = generator()->CreateItem();
  store_test_util()->InsertItem(item2);

  test_clock.Advance(base::TimeDelta::FromSeconds(-1));
  OfflinePageThumbnail thumb2(item2.offline_id,
                              OfflineTimeNow() + kVisualsExpirationDelta,
                              "thumb2", "favicon2");
  StoreVisuals(thumb2.offline_id, thumb2.thumbnail, thumb2.favicon);

  // 3. No item, not expired.
  test_clock.Advance(base::TimeDelta::FromSeconds(1));
  OfflinePageThumbnail thumb3(store_utils::GenerateOfflineId(),
                              OfflineTimeNow() + kVisualsExpirationDelta,
                              "thumb3", "favicon3");
  StoreVisuals(thumb3.offline_id, thumb3.thumbnail, thumb3.favicon);

  // 4. No item, expired. This one gets removed.
  test_clock.Advance(base::TimeDelta::FromSeconds(-1));
  OfflinePageThumbnail thumb4(store_utils::GenerateOfflineId(),
                              OfflineTimeNow() + kVisualsExpirationDelta,
                              "thumb4", "favicon4");
  StoreVisuals(thumb4.offline_id, thumb4.thumbnail, thumb4.favicon);

  base::MockCallback<CleanupThumbnailsCallback> callback;
  EXPECT_CALL(callback, Run(true)).Times(1);

  test_clock.Advance(kVisualsExpirationDelta + base::TimeDelta::FromSeconds(1));

  base::HistogramTester histogram_tester;
  RunTask(std::make_unique<CleanupThumbnailsTask>(store(), OfflineTimeNow(),
                                                  callback.Get()));
  EXPECT_EQ(thumb1, MustReadThumbnail(thumb1.offline_id));
  EXPECT_EQ(thumb2, MustReadThumbnail(thumb2.offline_id));
  EXPECT_EQ(thumb3, MustReadThumbnail(thumb3.offline_id));
  EXPECT_EQ(nullptr, ReadThumbnail(thumb4.offline_id).get());

  histogram_tester.ExpectUniqueSample("OfflinePages.CleanupThumbnails.Count", 1,
                                      1);
}

}  // namespace
}  // namespace offline_pages
