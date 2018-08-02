// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_specifics_conversions.h"

#include <memory>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::Eq;
using testing::NotNull;

namespace sync_bookmarks {

namespace {

TEST(BookmarkSpecificsConversionsTest, ShouldCreateSpecificsFromBookmarkNode) {
  const GURL kUrl("http://www.url.com");
  const std::string kTitle = "Title";
  const base::Time kTime = base::Time::Now();
  const GURL kIconUrl("http://www.icon-url.com");
  const std::string kKey1 = "key1";
  const std::string kValue1 = "value1";
  const std::string kKey2 = "key2";
  const std::string kValue2 = "value2";

  bookmarks::BookmarkNode node(kUrl);
  node.SetTitle(base::UTF8ToUTF16(kTitle));
  node.set_date_added(kTime);
  node.SetMetaInfo(kKey1, kValue1);
  node.SetMetaInfo(kKey2, kValue2);

  sync_pb::EntitySpecifics specifics = CreateSpecificsFromBookmarkNode(&node);
  const sync_pb::BookmarkSpecifics& bm_specifics = specifics.bookmark();
  EXPECT_THAT(bm_specifics.title(), Eq(kTitle));
  EXPECT_THAT(GURL(bm_specifics.url()), Eq(kUrl));
  EXPECT_THAT(
      base::Time::FromDeltaSinceWindowsEpoch(
          base::TimeDelta::FromMicroseconds(bm_specifics.creation_time_us())),
      Eq(kTime));
  for (const sync_pb::MetaInfo& meta_info : bm_specifics.meta_info()) {
    std::string value;
    node.GetMetaInfo(meta_info.key(), &value);
    EXPECT_THAT(meta_info.value(), Eq(value));
  }
}

TEST(BookmarkSpecificsConversionsTest,
     ShouldUpdateBookmarkNodeFromBookmarkSpecifics) {
  const GURL kUrl("http://www.url.com");
  const std::string kTitle = "Title";
  const base::Time kTime = base::Time::Now();
  const GURL kIconUrl("http://www.icon-url.com");
  const std::string kKey1 = "key1";
  const std::string kValue1 = "value1";
  const std::string kKey2 = "key2";
  const std::string kValue2 = "value2";

  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
  bm_specifics->set_url(kUrl.spec());
  bm_specifics->set_title(kTitle);
  bm_specifics->set_creation_time_us(
      kTime.ToDeltaSinceWindowsEpoch().InMicroseconds());
  sync_pb::MetaInfo* meta_info1 = bm_specifics->add_meta_info();
  meta_info1->set_key(kKey1);
  meta_info1->set_value(kValue1);

  sync_pb::MetaInfo* meta_info2 = bm_specifics->add_meta_info();
  meta_info2->set_key(kKey2);
  meta_info2->set_value(kValue2);

  std::unique_ptr<bookmarks::BookmarkModel> model =
      bookmarks::TestBookmarkClient::CreateModel();
  const bookmarks::BookmarkNode* node = CreateBookmarkNodeFromSpecifics(
      *bm_specifics,
      /*parent=*/model->bookmark_bar_node(), /*index=*/0,
      /*is_folder=*/false, model.get());
  ASSERT_THAT(node, NotNull());
  EXPECT_THAT(node->GetTitle(), Eq(base::UTF8ToUTF16(kTitle)));
  EXPECT_THAT(node->url(), Eq(kUrl));
  EXPECT_THAT(node->date_added(), Eq(kTime));
  std::string value1;
  node->GetMetaInfo(kKey1, &value1);
  EXPECT_THAT(value1, Eq(kValue1));
  std::string value2;
  node->GetMetaInfo(kKey2, &value2);
  EXPECT_THAT(value2, Eq(kValue2));
}

TEST(BookmarkSpecificsConversionsTest, ShouldBeValidBookmarkSpecifics) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();

  // URL is irrelevant for a folder.
  bm_specifics->set_url("INVALID_URL");
  EXPECT_TRUE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/true));

  bm_specifics->set_url("http://www.valid-url.com");
  EXPECT_TRUE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/false));
}

TEST(BookmarkSpecificsConversionsTest, ShouldBeInvalidBookmarkSpecifics) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
  // Empty specifics.
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/false));
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/true));

  // Add invalid url.
  bm_specifics->set_url("INVALID_URL");
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/false));

  // Add a valid url.
  bm_specifics->set_url("http://www.valid-url.com");
  // Add redudant keys in meta_info.
  sync_pb::MetaInfo* meta_info1 = bm_specifics->add_meta_info();
  meta_info1->set_key("key");
  meta_info1->set_value("value1");

  sync_pb::MetaInfo* meta_info2 = bm_specifics->add_meta_info();
  meta_info2->set_key("key");
  meta_info2->set_value("value2");
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/false));
  EXPECT_FALSE(IsValidBookmarkSpecifics(*bm_specifics, /*is_folder=*/true));
}

}  // namespace

}  // namespace sync_bookmarks
