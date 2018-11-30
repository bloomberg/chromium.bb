// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/get_thumbnail_info_task.h"

#include "base/test/mock_callback.h"
#include "components/offline_pages/core/prefetch/tasks/prefetch_task_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {
const char kTestUrl[] = "https://www.test_thumbnail.com/";

class GetThumbnailInfoTaskTest : public PrefetchTaskTestBase {
 public:
  GetThumbnailInfoTaskTest() = default;
  ~GetThumbnailInfoTaskTest() override = default;

  PrefetchItem TestItem() {
    PrefetchItem item =
        item_generator()->CreateItem(PrefetchItemState::DOWNLOADED);
    item.thumbnail_url = GURL(kTestUrl);
    return item;
  }
};

MATCHER(IsNullResult, "") {
  return arg.thumbnail_url.is_empty();
}

MATCHER(IsTestUrl, "") {
  return arg.thumbnail_url.possibly_invalid_spec() == kTestUrl;
}

TEST_F(GetThumbnailInfoTaskTest, NotPresent) {
  const PrefetchItem item = TestItem();
  store_util()->InsertPrefetchItem(item);

  base::MockCallback<GetThumbnailInfoTask::ResultCallback> callback;
  EXPECT_CALL(callback, Run(IsNullResult()));
  RunTask(std::make_unique<GetThumbnailInfoTask>(store(), item.offline_id + 1,
                                                 callback.Get()));
}

TEST_F(GetThumbnailInfoTaskTest, Found) {
  const PrefetchItem item = TestItem();
  store_util()->InsertPrefetchItem(item);

  base::MockCallback<GetThumbnailInfoTask::ResultCallback> callback;
  EXPECT_CALL(callback, Run(IsTestUrl()));
  RunTask(std::make_unique<GetThumbnailInfoTask>(store(), item.offline_id,
                                                 callback.Get()));
}

}  // namespace
}  // namespace offline_pages
