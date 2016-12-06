// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

#include <utility>

#include "components/arc/common/intent_helper.mojom.h"
#include "components/arc/intent_helper/activity_icon_loader.h"
#include "components/arc/intent_helper/local_activity_resolver.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

namespace {

class ArcIntentHelperTest : public testing::Test {
 public:
  ArcIntentHelperTest() = default;

 protected:
  std::unique_ptr<FakeArcBridgeService> fake_arc_bridge_service_;
  scoped_refptr<ActivityIconLoader> icon_loader_;
  scoped_refptr<LocalActivityResolver> activity_resolver_;
  std::unique_ptr<ArcIntentHelperBridge> instance_;

 private:
  void SetUp() override {
    fake_arc_bridge_service_.reset(new FakeArcBridgeService());
    icon_loader_ = new ActivityIconLoader();
    activity_resolver_ = new LocalActivityResolver();
    instance_.reset(new ArcIntentHelperBridge(
        fake_arc_bridge_service_.get(), icon_loader_, activity_resolver_));
  }

  void TearDown() override {
    instance_.reset();
    activity_resolver_ = nullptr;
    icon_loader_ = nullptr;
    fake_arc_bridge_service_.reset();
  }

  DISALLOW_COPY_AND_ASSIGN(ArcIntentHelperTest);
};

}  // namespace

// Tests if IsIntentHelperPackage works as expected. Probably too trivial
// to test but just in case.
TEST_F(ArcIntentHelperTest, TestIsIntentHelperPackage) {
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
TEST_F(ArcIntentHelperTest, TestFilterOutIntentHelper) {
  {
    std::vector<mojom::IntentHandlerInfoPtr> orig;
    std::vector<mojom::IntentHandlerInfoPtr> filtered =
        ArcIntentHelperBridge::FilterOutIntentHelper(std::move(orig));
    EXPECT_EQ(0U, filtered.size());
  }

  {
    std::vector<mojom::IntentHandlerInfoPtr> orig;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[0]->name = "0";
    orig[0]->package_name = "package_name0";
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[1]->name = "1";
    orig[1]->package_name = "package_name1";

    // FilterOutIntentHelper is no-op in this case.
    std::vector<mojom::IntentHandlerInfoPtr> filtered =
        ArcIntentHelperBridge::FilterOutIntentHelper(std::move(orig));
    EXPECT_EQ(2U, filtered.size());
  }

  {
    std::vector<mojom::IntentHandlerInfoPtr> orig;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[0]->name = "0";
    orig[0]->package_name = ArcIntentHelperBridge::kArcIntentHelperPackageName;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[1]->name = "1";
    orig[1]->package_name = "package_name1";

    // FilterOutIntentHelper should remove the first element.
    std::vector<mojom::IntentHandlerInfoPtr> filtered =
        ArcIntentHelperBridge::FilterOutIntentHelper(std::move(orig));
    ASSERT_EQ(1U, filtered.size());
    EXPECT_EQ("1", filtered[0]->name);
    EXPECT_EQ("package_name1", filtered[0]->package_name);
  }

  {
    std::vector<mojom::IntentHandlerInfoPtr> orig;
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
    std::vector<mojom::IntentHandlerInfoPtr> filtered =
        ArcIntentHelperBridge::FilterOutIntentHelper(std::move(orig));
    ASSERT_EQ(1U, filtered.size());
    EXPECT_EQ("1", filtered[0]->name);
    EXPECT_EQ("package_name1", filtered[0]->package_name);
  }

  {
    std::vector<mojom::IntentHandlerInfoPtr> orig;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[0]->name = "0";
    orig[0]->package_name = ArcIntentHelperBridge::kArcIntentHelperPackageName;
    orig.push_back(mojom::IntentHandlerInfo::New());
    orig[1]->name = "1";
    orig[1]->package_name = ArcIntentHelperBridge::kArcIntentHelperPackageName;

    // FilterOutIntentHelper should remove all elements.
    std::vector<mojom::IntentHandlerInfoPtr> filtered =
        ArcIntentHelperBridge::FilterOutIntentHelper(std::move(orig));
    EXPECT_EQ(0U, filtered.size());
  }
}

// Tests if observer works as expected.
TEST_F(ArcIntentHelperTest, TestObserver) {
  class FakeObserver : public ArcIntentHelperObserver {
   public:
    FakeObserver() = default;
    void OnAppsUpdated() override { updated_ = true; }
    bool IsUpdated() { return updated_; }
    void Reset() { updated_ = false; }

   private:
    bool updated_ = false;
  };

  // Observer should be called when intent filter is updated.
  auto observer = base::MakeUnique<FakeObserver>();
  instance_->AddObserver(observer.get());
  EXPECT_FALSE(observer->IsUpdated());
  instance_->OnIntentFiltersUpdated(std::vector<mojom::IntentFilterPtr>());
  EXPECT_TRUE(observer->IsUpdated());

  // Observer should not be called after it's removed.
  observer->Reset();
  instance_->RemoveObserver(observer.get());
  instance_->OnIntentFiltersUpdated(std::vector<mojom::IntentFilterPtr>());
  EXPECT_FALSE(observer->IsUpdated());
}

}  // namespace arc
