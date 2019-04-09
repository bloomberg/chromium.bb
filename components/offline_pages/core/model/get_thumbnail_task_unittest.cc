// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/get_thumbnail_task.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/model/offline_page_item_generator.h"
#include "components/offline_pages/core/model/store_visuals_task.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_page_metadata_store_test_util.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"
#include "components/offline_pages/task/test_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace offline_pages {
namespace {

OfflinePageThumbnail TestThumbnail(base::Time now) {
  OfflinePageThumbnail thumb;
  thumb.offline_id = 1;
  thumb.expiration = now + kVisualsExpirationDelta;
  thumb.thumbnail = "123abc";
  thumb.favicon = "favicon";
  return thumb;
}

class GetThumbnailTaskTest : public ModelTaskTestBase {
 public:
  ~GetThumbnailTaskTest() override = default;

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

  void StoreVisuals(const OfflinePageThumbnail& thumb) {
    RunTask(StoreVisualsTask::MakeStoreThumbnailTask(
        store(), thumb.offline_id, thumb.thumbnail, base::DoNothing()));
    RunTask(StoreVisualsTask::MakeStoreFaviconTask(
        store(), thumb.offline_id, thumb.favicon, base::DoNothing()));
  }
};

TEST_F(GetThumbnailTaskTest, NotFound) {
  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<OfflinePageThumbnail> result) {
        called = true;
        EXPECT_FALSE(result);
      });

  RunTask(std::make_unique<GetThumbnailTask>(store(), 1, std::move(callback)));
  EXPECT_TRUE(called);
}

TEST_F(GetThumbnailTaskTest, Found) {
  TestScopedOfflineClock test_clock;
  const OfflinePageThumbnail thumb = TestThumbnail(OfflineTimeNow());
  StoreVisuals(thumb);

  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<OfflinePageThumbnail> result) {
        called = true;
        ASSERT_TRUE(result);
        EXPECT_EQ(thumb, *result);
      });

  RunTask(
      std::make_unique<GetThumbnailTask>(store(), thumb.offline_id, callback));
  EXPECT_TRUE(called);
}

TEST_F(GetThumbnailTaskTest, FoundThumbnailOnly) {
  TestScopedOfflineClock test_clock;
  OfflinePageThumbnail thumb = TestThumbnail(OfflineTimeNow());
  thumb.favicon = std::string();
  StoreVisuals(thumb);

  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<OfflinePageThumbnail> result) {
        called = true;
        ASSERT_TRUE(result);
        EXPECT_EQ(thumb, *result);
      });

  RunTask(
      std::make_unique<GetThumbnailTask>(store(), thumb.offline_id, callback));
  EXPECT_TRUE(called);
}

TEST_F(GetThumbnailTaskTest, FoundFaviconOnly) {
  TestScopedOfflineClock test_clock;
  OfflinePageThumbnail thumb = TestThumbnail(OfflineTimeNow());
  thumb.thumbnail = std::string();
  StoreVisuals(thumb);

  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<OfflinePageThumbnail> result) {
        called = true;
        ASSERT_TRUE(result);
        EXPECT_EQ(thumb, *result);
      });

  RunTask(
      std::make_unique<GetThumbnailTask>(store(), thumb.offline_id, callback));
  EXPECT_TRUE(called);
}

TEST_F(GetThumbnailTaskTest, DbConnectionIsNull) {
  RunTask(StoreVisualsTask::MakeStoreThumbnailTask(store(), 1, std::string(),
                                                   base::DoNothing()));

  bool called = false;
  auto callback = base::BindLambdaForTesting(
      [&](std::unique_ptr<OfflinePageThumbnail> result) {
        called = true;
        EXPECT_FALSE(result);
      });
  store()->SetInitializationStatusForTesting(
      SqlStoreBase::InitializationStatus::kFailure, true);

  RunTask(std::make_unique<GetThumbnailTask>(store(), 1, std::move(callback)));

  EXPECT_TRUE(called);
}

}  // namespace
}  // namespace offline_pages
