// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_struct_traits.h"

#include "ash/public/cpp/shelf_item.h"
#include "ash/public/interfaces/shelf.mojom.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/common/common_custom_types_struct_traits.h"
#include "skia/public/interfaces/bitmap_skbitmap_struct_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {
namespace {

TEST(ShelfItemStructTraitsTest, Basic) {
  ShelfItem item;
  item.type = TYPE_APP;
  item.image = gfx::test::CreateImageSkia(32, 16);
  item.id = 123u;
  item.status = STATUS_RUNNING;
  item.app_launch_id = AppLaunchId("app_id", "launch_id");
  item.title = base::ASCIIToUTF16("title");
  item.shows_tooltip = false;
  item.pinned_by_policy = true;

  ShelfItem out_item;
  ASSERT_TRUE(mojom::ShelfItem::Deserialize(mojom::ShelfItem::Serialize(&item),
                                            &out_item));

  EXPECT_EQ(TYPE_APP, out_item.type);
  EXPECT_FALSE(out_item.image.isNull());
  EXPECT_EQ(gfx::Size(32, 16), out_item.image.size());
  EXPECT_EQ(123u, out_item.id);
  EXPECT_EQ(STATUS_RUNNING, out_item.status);
  EXPECT_EQ("app_id", out_item.app_launch_id.app_id());
  EXPECT_EQ("launch_id", out_item.app_launch_id.launch_id());
  EXPECT_EQ(base::ASCIIToUTF16("title"), out_item.title);
  EXPECT_FALSE(out_item.shows_tooltip);
  EXPECT_TRUE(out_item.pinned_by_policy);
}

}  // namespace
}  // namespace ash