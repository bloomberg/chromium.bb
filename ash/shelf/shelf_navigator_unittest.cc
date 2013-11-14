// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_navigator.h"

#include "ash/ash_switches.h"
#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_types.h"
#include "ash/shelf/shelf_model.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class LauncherNavigatorTest : public testing::Test {
 public:
  LauncherNavigatorTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    model_.reset(new ShelfModel);

    // Add APP_LIST for test.
    LauncherItem app_list;
    app_list.type = TYPE_APP_LIST;
    model_->Add(app_list);

    // Initially, applist launcher item is only created.
    int total_num = model_->item_count();
    EXPECT_EQ(1, total_num);
    EXPECT_TRUE(model_->items()[0].type == TYPE_APP_LIST);

    // Add BROWSER_SHORTCUT for test.
    LauncherItem browser_shortcut;
    browser_shortcut.type = TYPE_BROWSER_SHORTCUT;
    model_->Add(browser_shortcut);
  }

  void SetupMockShelfModel(LauncherItemType* types,
                              int types_length,
                              int focused_index) {
    for (int i = 0; i < types_length; ++i) {
      LauncherItem new_item;
      new_item.type = types[i];
      new_item.status =
          (types[i] == TYPE_PLATFORM_APP) ? STATUS_RUNNING : STATUS_CLOSED;
      model_->Add(new_item);
    }

    // Set the focused item.
    if (focused_index >= 0) {
      LauncherItem focused_item =model_->items()[focused_index];
      if (focused_item.type == TYPE_PLATFORM_APP) {
        focused_item.status = STATUS_ACTIVE;
        model_->Set(focused_index, focused_item);
      }
    }
  }

  const ShelfModel& model() { return *model_.get(); }

 private:
  scoped_ptr<ShelfModel> model_;

  DISALLOW_COPY_AND_ASSIGN(LauncherNavigatorTest);
};

class LauncherNavigatorLegacyShelfLayoutTest : public LauncherNavigatorTest {
 public:
  LauncherNavigatorLegacyShelfLayoutTest() : LauncherNavigatorTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kAshDisableAlternateShelfLayout);
    LauncherNavigatorTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherNavigatorLegacyShelfLayoutTest);
};

}  // namespace

TEST_F(LauncherNavigatorTest, BasicCycle) {
  // An app shortcut and three platform apps.
  LauncherItemType types[] = {
    TYPE_APP_SHORTCUT, TYPE_PLATFORM_APP, TYPE_PLATFORM_APP, TYPE_PLATFORM_APP,
  };
  // ShelfModel automatically adds BROWSER_SHORTCUT item at the
  // beginning, so '3' refers the first TYPE_PLATFORM_APP item.
  SetupMockShelfModel(types, arraysize(types), 3);

  EXPECT_EQ(4, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));

  // Fourth one.  It will skip the APP_SHORTCUT at the beginning of the list and
  // the APP_LIST item which is automatically added at the end of items.
  EXPECT_EQ(5, GetNextActivatedItemIndex(model(), CYCLE_BACKWARD));
}

TEST_F(LauncherNavigatorLegacyShelfLayoutTest, BasicCycle) {
  // An app shortcut and three platform apps.
  LauncherItemType types[] = {
    TYPE_APP_SHORTCUT, TYPE_PLATFORM_APP, TYPE_PLATFORM_APP, TYPE_PLATFORM_APP,
  };
  // ShelfModel automatically adds BROWSER_SHORTCUT item at the
  // beginning, so '2' refers the first TYPE_PLATFORM_APP item.
  SetupMockShelfModel(types, arraysize(types), 2);

  EXPECT_EQ(3, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));

  // Fourth one.  It will skip the APP_SHORTCUT at the beginning of the list and
  // the APP_LIST item which is automatically added at the end of items.
  EXPECT_EQ(4, GetNextActivatedItemIndex(model(), CYCLE_BACKWARD));
}

TEST_F(LauncherNavigatorTest, WrapToBeginning) {
  LauncherItemType types[] = {
    TYPE_APP_SHORTCUT, TYPE_PLATFORM_APP, TYPE_PLATFORM_APP, TYPE_PLATFORM_APP,
  };
  SetupMockShelfModel(types, arraysize(types), 5);

  // Second one.  It skips the APP_LIST item at the end of the list,
  // wraps to the beginning, and skips BROWSER_SHORTCUT and APP_SHORTCUT
  // at the beginning of the list.
  EXPECT_EQ(3, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));
}

TEST_F(LauncherNavigatorLegacyShelfLayoutTest, WrapToBeginning) {
  LauncherItemType types[] = {
    TYPE_APP_SHORTCUT, TYPE_PLATFORM_APP, TYPE_PLATFORM_APP, TYPE_PLATFORM_APP,
  };
  SetupMockShelfModel(types, arraysize(types), 4);

  // Second one.  It skips the APP_LIST item at the end of the list,
  // wraps to the beginning, and skips BROWSER_SHORTCUT and APP_SHORTCUT
  // at the beginning of the list.
  EXPECT_EQ(2, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));
}

TEST_F(LauncherNavigatorTest, Empty) {
  SetupMockShelfModel(NULL, 0, -1);
  EXPECT_EQ(-1, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));
  EXPECT_EQ(-1, GetNextActivatedItemIndex(model(), CYCLE_BACKWARD));
}

TEST_F(LauncherNavigatorTest, SingleEntry) {
  LauncherItemType type = TYPE_PLATFORM_APP;
  SetupMockShelfModel(&type, 1, 2);

  // If there's only one item there and it is already active, there's no item
  // to be activated next.
  EXPECT_EQ(-1, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));
  EXPECT_EQ(-1, GetNextActivatedItemIndex(model(), CYCLE_BACKWARD));
}

TEST_F(LauncherNavigatorLegacyShelfLayoutTest, SingleEntry) {
  LauncherItemType type = TYPE_PLATFORM_APP;
  SetupMockShelfModel(&type, 1, 1);

  // If there's only one item there and it is already active, there's no item
  // to be activated next.
  EXPECT_EQ(-1, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));
  EXPECT_EQ(-1, GetNextActivatedItemIndex(model(), CYCLE_BACKWARD));
}

TEST_F(LauncherNavigatorTest, NoActive) {
  LauncherItemType types[] = {
    TYPE_PLATFORM_APP, TYPE_PLATFORM_APP,
  };
  // Special case: no items are 'STATUS_ACTIVE'.
  SetupMockShelfModel(types, arraysize(types), -1);

  // If there are no active status, pick the first running item as a fallback.
  EXPECT_EQ(2, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));
  EXPECT_EQ(2, GetNextActivatedItemIndex(model(), CYCLE_BACKWARD));
}

TEST_F(LauncherNavigatorLegacyShelfLayoutTest, NoActive) {
  LauncherItemType types[] = {
    TYPE_PLATFORM_APP, TYPE_PLATFORM_APP,
  };
  // Special case: no items are 'STATUS_ACTIVE'.
  SetupMockShelfModel(types, arraysize(types), -1);

  // If there are no active status, pick the first running item as a fallback.
  EXPECT_EQ(1, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));
  EXPECT_EQ(1, GetNextActivatedItemIndex(model(), CYCLE_BACKWARD));
}

}  // namespace ash
