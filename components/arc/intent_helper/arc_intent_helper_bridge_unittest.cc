// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

#include <utility>

#include "components/arc/common/intent_helper.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

// Tests if IsIntentHelperPackage works as expected. Probably too trivial
// to test but just in case.
TEST(ArcIntentHelperTest, TestIsIntentHelperPackage) {
  EXPECT_FALSE(ArcIntentHelperBridge::IsIntentHelperPackage(""));
  EXPECT_FALSE(ArcIntentHelperBridge::IsIntentHelperPackage(
      ArcIntentHelperBridge::kArcIntentHelperPackageName + std::string("a")));
  EXPECT_FALSE(ArcIntentHelperBridge::IsIntentHelperPackage(
      ArcIntentHelperBridge::kArcIntentHelperPackageName +
      std::string("/.ArcIntentHelperActivity")));
  EXPECT_TRUE(ArcIntentHelperBridge::IsIntentHelperPackage(
      ArcIntentHelperBridge::kArcIntentHelperPackageName));
}

// Tests if FilterOutIntentHelper removes handlers as expected.
TEST(ArcIntentHelperTest, TestFilterOutIntentHelper) {
  {
    mojo::Array<mojom::IntentHandlerInfoPtr> orig;
    mojo::Array<mojom::IntentHandlerInfoPtr> filtered =
        ArcIntentHelperBridge::FilterOutIntentHelper(std::move(orig));
    EXPECT_EQ(0U, filtered.size());
  }

  {
    mojo::Array<mojom::IntentHandlerInfoPtr> orig;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[0]->name = "0";
    orig[0]->package_name = "package_name0";
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[1]->name = "1";
    orig[1]->package_name = "package_name1";

    // FilterOutIntentHelper is no-op in this case.
    mojo::Array<mojom::IntentHandlerInfoPtr> filtered =
        ArcIntentHelperBridge::FilterOutIntentHelper(std::move(orig));
    EXPECT_EQ(2U, filtered.size());
  }

  {
    mojo::Array<mojom::IntentHandlerInfoPtr> orig;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[0]->name = "0";
    orig[0]->package_name = ArcIntentHelperBridge::kArcIntentHelperPackageName;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[1]->name = "1";
    orig[1]->package_name = "package_name1";

    // FilterOutIntentHelper should remove the first element.
    mojo::Array<mojom::IntentHandlerInfoPtr> filtered =
        ArcIntentHelperBridge::FilterOutIntentHelper(std::move(orig));
    ASSERT_EQ(1U, filtered.size());
    EXPECT_EQ("1", filtered[0]->name);
    EXPECT_EQ("package_name1", filtered[0]->package_name);
  }

  {
    mojo::Array<mojom::IntentHandlerInfoPtr> orig;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[0]->name = "0";
    orig[0]->package_name = ArcIntentHelperBridge::kArcIntentHelperPackageName;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[1]->name = "1";
    orig[1]->package_name = "package_name1";
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[2]->name = "2";
    orig[2]->package_name = ArcIntentHelperBridge::kArcIntentHelperPackageName;

    // FilterOutIntentHelper should remove two elements.
    mojo::Array<mojom::IntentHandlerInfoPtr> filtered =
        ArcIntentHelperBridge::FilterOutIntentHelper(std::move(orig));
    ASSERT_EQ(1U, filtered.size());
    EXPECT_EQ("1", filtered[0]->name);
    EXPECT_EQ("package_name1", filtered[0]->package_name);
  }

  {
    mojo::Array<mojom::IntentHandlerInfoPtr> orig;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[0]->name = "0";
    orig[0]->package_name = ArcIntentHelperBridge::kArcIntentHelperPackageName;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[1]->name = "1";
    orig[1]->package_name = ArcIntentHelperBridge::kArcIntentHelperPackageName;

    // FilterOutIntentHelper should remove all elements.
    mojo::Array<mojom::IntentHandlerInfoPtr> filtered =
        ArcIntentHelperBridge::FilterOutIntentHelper(std::move(orig));
    EXPECT_EQ(0U, filtered.size());
  }
}

}  // namespace arc
