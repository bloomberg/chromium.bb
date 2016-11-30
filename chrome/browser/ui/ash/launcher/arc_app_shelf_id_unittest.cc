// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/arc_app_shelf_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
constexpr char kTestShelfGroupId[] = "test_shelf_group_id";
constexpr char kTestAppId[] = "mconboelelhjpkbdhhiijkgcimoangdj";
constexpr char kTestIntentWithShelfGroup[] =
    "#Intent;S.org.chromium.arc.shelf_group_id=test_shelf_group_id;"
    "S.other=tmp;end";
constexpr char kTestIntentWithoutShelfGroup[] = "#Intent;S.other=tmp;end";
}  // namespace

class ArcAppShelfIdTest : public testing::Test {
 public:
  ArcAppShelfIdTest() = default;
  ~ArcAppShelfIdTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcAppShelfIdTest);
};

TEST_F(ArcAppShelfIdTest, BaseTestAAA) {
  const arc::ArcAppShelfId shelf_id1(kTestShelfGroupId, kTestAppId);
  EXPECT_TRUE(shelf_id1.has_shelf_group_id());
  EXPECT_EQ(shelf_id1.shelf_group_id(), kTestShelfGroupId);
  EXPECT_EQ(shelf_id1.app_id(), kTestAppId);

  const arc::ArcAppShelfId shelf_id2 =
      arc::ArcAppShelfId::FromString(shelf_id1.ToString());
  EXPECT_EQ(shelf_id1.shelf_group_id(), shelf_id2.shelf_group_id());
  EXPECT_EQ(shelf_id1.app_id(), shelf_id2.app_id());

  const arc::ArcAppShelfId shelf_id3(std::string(), kTestAppId);
  EXPECT_FALSE(shelf_id3.has_shelf_group_id());
  EXPECT_EQ(shelf_id3.app_id(), kTestAppId);

  const arc::ArcAppShelfId shelf_id4 =
      arc::ArcAppShelfId::FromString(shelf_id3.ToString());
  EXPECT_EQ(shelf_id3.shelf_group_id(), shelf_id4.shelf_group_id());
  EXPECT_EQ(shelf_id3.app_id(), shelf_id4.app_id());

  const arc::ArcAppShelfId shelf_id5 = arc::ArcAppShelfId::FromIntentAndAppId(
      kTestIntentWithShelfGroup, kTestAppId);
  EXPECT_TRUE(shelf_id5.has_shelf_group_id());
  EXPECT_EQ(shelf_id5.shelf_group_id(), kTestShelfGroupId);
  EXPECT_EQ(shelf_id5.app_id(), kTestAppId);

  const arc::ArcAppShelfId shelf_id6 = arc::ArcAppShelfId::FromIntentAndAppId(
      kTestIntentWithoutShelfGroup, kTestAppId);
  EXPECT_FALSE(shelf_id6.has_shelf_group_id());
  EXPECT_EQ(shelf_id6.app_id(), kTestAppId);

  const arc::ArcAppShelfId shelf_id7 =
      arc::ArcAppShelfId::FromIntentAndAppId(std::string(), kTestAppId);
  EXPECT_FALSE(shelf_id7.has_shelf_group_id());
  EXPECT_EQ(shelf_id7.app_id(), kTestAppId);
}
