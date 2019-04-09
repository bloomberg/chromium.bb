// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/has_thumbnail_task.h"

#include <memory>

#include "base/bind_helpers.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "components/offline_pages/core/model/model_task_test_base.h"
#include "components/offline_pages/core/model/store_visuals_task.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {

OfflinePageThumbnail TestThumbnail() {
  return OfflinePageThumbnail(1, store_utils::FromDatabaseTime(1234),
                              "some thumbnail", "some favicon");
}

using ThumbnailExistsCallback = HasThumbnailTask::ThumbnailExistsCallback;

class HasThumbnailTaskTest : public ModelTaskTestBase {
 public:
  void StoreVisuals(const OfflinePageThumbnail& thumb) {
    RunTask(StoreVisualsTask::MakeStoreThumbnailTask(
        store(), thumb.offline_id, thumb.thumbnail, base::DoNothing()));
    RunTask(StoreVisualsTask::MakeStoreFaviconTask(
        store(), thumb.offline_id, thumb.favicon, base::DoNothing()));
  }
};

TEST_F(HasThumbnailTaskTest, CorrectlyFindsById_ThumbnailAndFavicon) {
  OfflinePageThumbnail thumb = TestThumbnail();
  StoreVisuals(thumb);

  base::MockCallback<ThumbnailExistsCallback> exists_callback;
  VisualsAvailability availability = {true, true};
  EXPECT_CALL(exists_callback, Run(availability));
  RunTask(std::make_unique<HasThumbnailTask>(store(), thumb.offline_id,
                                             exists_callback.Get()));
}

TEST_F(HasThumbnailTaskTest, CorrectlyFindsById_ThumbnailOnly) {
  OfflinePageThumbnail thumb = TestThumbnail();
  thumb.favicon = std::string();
  StoreVisuals(thumb);

  base::MockCallback<ThumbnailExistsCallback> exists_callback;
  VisualsAvailability availability = {true, false};
  EXPECT_CALL(exists_callback, Run(availability));
  RunTask(std::make_unique<HasThumbnailTask>(store(), thumb.offline_id,
                                             exists_callback.Get()));
}

TEST_F(HasThumbnailTaskTest, CorrectlyFindsById_FaviconOnly) {
  OfflinePageThumbnail thumb = TestThumbnail();
  thumb.thumbnail = std::string();
  StoreVisuals(thumb);

  base::MockCallback<ThumbnailExistsCallback> exists_callback;
  VisualsAvailability availability = {false, true};
  EXPECT_CALL(exists_callback, Run(availability));
  RunTask(std::make_unique<HasThumbnailTask>(store(), thumb.offline_id,
                                             exists_callback.Get()));
}

TEST_F(HasThumbnailTaskTest, RowDoesNotExist) {
  const int64_t invalid_offline_id = 2;

  base::MockCallback<ThumbnailExistsCallback> doesnt_exist_callback;
  VisualsAvailability availability = {false, false};
  EXPECT_CALL(doesnt_exist_callback, Run(availability));
  RunTask(std::make_unique<HasThumbnailTask>(store(), invalid_offline_id,
                                             doesnt_exist_callback.Get()));
}

TEST_F(HasThumbnailTaskTest, DbConnectionIsNull) {
  OfflinePageThumbnail thumb;
  thumb.offline_id = 1;
  thumb.expiration = store_utils::FromDatabaseTime(1234);
  thumb.thumbnail = "123abc";
  StoreVisuals(thumb);

  store()->SetInitializationStatusForTesting(
      SqlStoreBase::InitializationStatus::kFailure, true);
  base::MockCallback<ThumbnailExistsCallback> exists_callback;
  VisualsAvailability availability = {false, false};
  EXPECT_CALL(exists_callback, Run(availability));
  RunTask(std::make_unique<HasThumbnailTask>(store(), thumb.offline_id,
                                             exists_callback.Get()));
}

}  // namespace
}  // namespace offline_pages
