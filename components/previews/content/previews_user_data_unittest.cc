// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_user_data.h"

#include <stdint.h>

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace previews {

TEST(PreviewsUserDataTest, TestConstructor) {
  uint64_t id = 5u;
  std::unique_ptr<PreviewsUserData> data(new PreviewsUserData(5u));
  EXPECT_EQ(id, data->page_id());
}

TEST(PreviewsUserDataTest, DeepCopy) {
  uint64_t id = 4u;
  std::unique_ptr<PreviewsUserData> data =
      std::make_unique<PreviewsUserData>(id);
  EXPECT_EQ(id, data->page_id());

  EXPECT_EQ(0, data->data_savings_inflation_percent());
  EXPECT_FALSE(data->cache_control_no_transform_directive());
  EXPECT_EQ(previews::PreviewsType::NONE, data->committed_previews_type());
  EXPECT_FALSE(data->black_listed_for_lite_page());
  EXPECT_FALSE(data->offline_preview_used());
  EXPECT_EQ(data->server_lite_page_info(), nullptr);

  base::TimeTicks now = base::TimeTicks::Now();

  data->set_data_savings_inflation_percent(123);
  data->set_cache_control_no_transform_directive();
  data->SetCommittedPreviewsType(previews::PreviewsType::NOSCRIPT);
  data->set_offline_preview_used(true);
  data->set_black_listed_for_lite_page(true);
  data->set_server_lite_page_info(
      std::make_unique<PreviewsUserData::ServerLitePageInfo>());
  data->server_lite_page_info()->original_navigation_start = now;

  PreviewsUserData data_copy(*data);
  EXPECT_EQ(id, data_copy.page_id());
  EXPECT_EQ(123, data_copy.data_savings_inflation_percent());
  EXPECT_TRUE(data_copy.cache_control_no_transform_directive());
  EXPECT_EQ(previews::PreviewsType::NOSCRIPT,
            data_copy.committed_previews_type());
  EXPECT_TRUE(data_copy.black_listed_for_lite_page());
  EXPECT_TRUE(data_copy.offline_preview_used());
  EXPECT_NE(data->server_lite_page_info(), nullptr);
  EXPECT_EQ(data->server_lite_page_info()->original_navigation_start, now);
}

}  // namespace previews
