// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/store_visuals_task.h"

#include <memory>

#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "components/offline_pages/core/model/get_thumbnail_task.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"

namespace offline_pages {
namespace {

const char kThumbnailData[] = "123abc";
const char kFaviconData[] = "favicon";
OfflinePageThumbnail CreateVisualsItem(base::Time now) {
  return OfflinePageThumbnail(1, now + kVisualsExpirationDelta, kThumbnailData,
                              kFaviconData);
}

class StoreVisualsTaskTest : public ModelTaskTestBase {
 public:
  ~StoreVisualsTaskTest() override {}

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
};

TEST_F(StoreVisualsTaskTest, Success) {
  TestScopedOfflineClock test_clock;
  OfflinePageThumbnail thumb = CreateVisualsItem(OfflineTimeNow());
  base::MockCallback<StoreVisualsTask::CompleteCallback> callback;
  EXPECT_CALL(callback, Run(true, thumb.thumbnail));
  EXPECT_CALL(callback, Run(true, thumb.favicon));

  RunTask(StoreVisualsTask::MakeStoreThumbnailTask(
      store(), thumb.offline_id, thumb.thumbnail, callback.Get()));
  RunTask(StoreVisualsTask::MakeStoreFaviconTask(
      store(), thumb.offline_id, thumb.favicon, callback.Get()));

  EXPECT_EQ(thumb, MustReadThumbnail(thumb.offline_id));
}

TEST_F(StoreVisualsTaskTest, AlreadyExists) {
  // Store the same thumbnail twice. The second operation should overwrite the
  // first.
  TestScopedOfflineClock test_clock;
  OfflinePageThumbnail thumb = CreateVisualsItem(OfflineTimeNow());
  base::MockCallback<StoreVisualsTask::CompleteCallback> callback;
  EXPECT_CALL(callback, Run(true, thumb.favicon));
  EXPECT_CALL(callback, Run(true, thumb.thumbnail));

  RunTask(StoreVisualsTask::MakeStoreThumbnailTask(
      store(), thumb.offline_id, thumb.thumbnail, callback.Get()));
  RunTask(StoreVisualsTask::MakeStoreFaviconTask(
      store(), thumb.offline_id, thumb.favicon, callback.Get()));

  EXPECT_EQ(thumb, MustReadThumbnail(thumb.offline_id));

  test_clock.Advance(base::TimeDelta::FromDays(1));
  thumb.thumbnail += "_extradata";
  thumb.expiration = OfflineTimeNow() + kVisualsExpirationDelta;
  EXPECT_CALL(callback, Run(true, thumb.thumbnail));

  RunTask(StoreVisualsTask::MakeStoreThumbnailTask(
      store(), thumb.offline_id, thumb.thumbnail, callback.Get()));

  OfflinePageThumbnail got_thumb = MustReadThumbnail(thumb.offline_id);
  EXPECT_EQ(thumb.thumbnail, got_thumb.thumbnail);
  EXPECT_EQ(thumb.expiration, got_thumb.expiration);
}

TEST_F(StoreVisualsTaskTest, IgnoreEmptyStrings) {
  TestScopedOfflineClock test_clock;
  OfflinePageThumbnail thumb = CreateVisualsItem(OfflineTimeNow());
  thumb.favicon = std::string();
  base::MockCallback<StoreVisualsTask::CompleteCallback> callback;
  EXPECT_CALL(callback, Run(true, thumb.thumbnail));
  EXPECT_CALL(callback, Run(true, std::string()));

  RunTask(StoreVisualsTask::MakeStoreThumbnailTask(
      store(), thumb.offline_id, thumb.thumbnail, callback.Get()));
  EXPECT_EQ(thumb, MustReadThumbnail(thumb.offline_id));

  RunTask(StoreVisualsTask::MakeStoreThumbnailTask(
      store(), thumb.offline_id, std::string(), callback.Get()));
  EXPECT_EQ(thumb.thumbnail, MustReadThumbnail(thumb.offline_id).thumbnail);
}

TEST_F(StoreVisualsTaskTest, DbConnectionIsNull) {
  store()->SetInitializationStatusForTesting(
      SqlStoreBase::InitializationStatus::kFailure, true);
  base::MockCallback<StoreVisualsTask::CompleteCallback> callback;
  EXPECT_CALL(callback, Run(false, kThumbnailData));
  RunTask(StoreVisualsTask::MakeStoreThumbnailTask(store(), 1, kThumbnailData,
                                                   callback.Get()));

  EXPECT_CALL(callback, Run(false, kFaviconData));
  RunTask(StoreVisualsTask::MakeStoreFaviconTask(store(), 1, kFaviconData,
                                                 callback.Get()));
}

}  // namespace
}  // namespace offline_pages
