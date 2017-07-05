// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_item.h"

#include "base/time/time.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {

TEST(PrefetchItemTest, OperatorEqualsAndCopyConstructor) {
  PrefetchItem item1;
  EXPECT_EQ(item1, PrefetchItem());
  EXPECT_EQ(item1, PrefetchItem(item1));

  item1.offline_id = 77L;
  EXPECT_NE(item1, PrefetchItem());
  EXPECT_EQ(item1, PrefetchItem(item1));
  item1 = PrefetchItem();

  item1.guid = "A";
  EXPECT_NE(item1, PrefetchItem());
  EXPECT_EQ(item1, PrefetchItem(item1));
  item1 = PrefetchItem();

  item1.client_id = ClientId("B", "C");
  EXPECT_NE(item1, PrefetchItem());
  EXPECT_EQ(item1, PrefetchItem(item1));
  item1 = PrefetchItem();

  item1.state = PrefetchItemState::AWAITING_GCM;
  EXPECT_NE(item1, PrefetchItem());
  EXPECT_EQ(item1, PrefetchItem(item1));
  item1 = PrefetchItem();

  item1.url = GURL("http://test.com");
  EXPECT_NE(item1, PrefetchItem());
  EXPECT_EQ(item1, PrefetchItem(item1));
  item1 = PrefetchItem();

  item1.final_archived_url = GURL("http://test.com/final");
  EXPECT_NE(item1, PrefetchItem());
  EXPECT_EQ(item1, PrefetchItem(item1));
  item1 = PrefetchItem();

  item1.request_archive_attempt_count = 10;
  EXPECT_NE(item1, PrefetchItem());
  EXPECT_EQ(item1, PrefetchItem(item1));
  item1 = PrefetchItem();

  item1.operation_name = "D";
  EXPECT_NE(item1, PrefetchItem());
  EXPECT_EQ(item1, PrefetchItem(item1));
  item1 = PrefetchItem();

  item1.archive_body_name = "E";
  EXPECT_NE(item1, PrefetchItem());
  EXPECT_EQ(item1, PrefetchItem(item1));
  item1 = PrefetchItem();

  item1.archive_body_length = 20;
  EXPECT_NE(item1, PrefetchItem());
  EXPECT_EQ(item1, PrefetchItem(item1));
  item1 = PrefetchItem();

  item1.creation_time = base::Time::FromJavaTime(1000L);
  EXPECT_NE(item1, PrefetchItem());
  EXPECT_EQ(item1, PrefetchItem(item1));
  item1 = PrefetchItem();

  item1.freshness_time = base::Time::FromJavaTime(2000L);
  EXPECT_NE(item1, PrefetchItem());
  EXPECT_EQ(item1, PrefetchItem(item1));
  item1 = PrefetchItem();

  item1.error_code = PrefetchItemErrorCode::EXPIRED;
  EXPECT_NE(item1, PrefetchItem());
  EXPECT_EQ(item1, PrefetchItem(item1));
}

}  // namespace offline_pages
