// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_navigator.h"

#include "ash/ash_switches.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_item_types.h"
#include "ash/shelf/shelf_model.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class ShelfNavigatorTest : public testing::Test {
 public:
  ShelfNavigatorTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    model_.reset(new ShelfModel);

    // Add APP_LIST for test.
    ShelfItem app_list;
    app_list.type = TYPE_APP_LIST;
    model_->Add(app_list);

    // Initially, applist launcher item is only created.
    int total_num = model_->item_count();
    EXPECT_EQ(1, total_num);
    EXPECT_TRUE(model_->items()[0].type == TYPE_APP_LIST);

    // Add BROWSER_SHORTCUT for test.
    ShelfItem browser_shortcut;
    browser_shortcut.type = TYPE_BROWSER_SHORTCUT;
    model_->Add(browser_shortcut);
  }

  void SetupMockShelfModel(ShelfItemType* types,
                           int types_length,
                           int focused_index) {
    for (int i = 0; i < types_length; ++i) {
      ShelfItem new_item;
      new_item.type = types[i];
      new_item.status =
          (types[i] == TYPE_PLATFORM_APP) ? STATUS_RUNNING : STATUS_CLOSED;
      model_->Add(new_item);
    }

    // Set the focused item.
    if (focused_index >= 0) {
      ShelfItem focused_item =model_->items()[focused_index];
      if (focused_item.type == TYPE_PLATFORM_APP) {
        focused_item.status = STATUS_ACTIVE;
        model_->Set(focused_index, focused_item);
      }
    }
  }

  const ShelfModel& model() { return *model_.get(); }

 private:
  scoped_ptr<ShelfModel> model_;

  DISALLOW_COPY_AND_ASSIGN(ShelfNavigatorTest);
};

}  // namespace

TEST_F(ShelfNavigatorTest, BasicCycle) {
  // An app shortcut and three platform apps.
  ShelfItemType types[] = {
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

TEST_F(ShelfNavigatorTest, WrapToBeginning) {
  ShelfItemType types[] = {
    TYPE_APP_SHORTCUT, TYPE_PLATFORM_APP, TYPE_PLATFORM_APP, TYPE_PLATFORM_APP,
  };
  SetupMockShelfModel(types, arraysize(types), 5);

  // Second one.  It skips the APP_LIST item at the end of the list,
  // wraps to the beginning, and skips BROWSER_SHORTCUT and APP_SHORTCUT
  // at the beginning of the list.
  EXPECT_EQ(3, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));
}

TEST_F(ShelfNavigatorTest, Empty) {
  SetupMockShelfModel(NULL, 0, -1);
  EXPECT_EQ(-1, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));
  EXPECT_EQ(-1, GetNextActivatedItemIndex(model(), CYCLE_BACKWARD));
}

TEST_F(ShelfNavigatorTest, SingleEntry) {
  ShelfItemType type = TYPE_PLATFORM_APP;
  SetupMockShelfModel(&type, 1, 2);

  // If there's only one item there and it is already active, there's no item
  // to be activated next.
  EXPECT_EQ(-1, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));
  EXPECT_EQ(-1, GetNextActivatedItemIndex(model(), CYCLE_BACKWARD));
}

TEST_F(ShelfNavigatorTest, NoActive) {
  ShelfItemType types[] = {
    TYPE_PLATFORM_APP, TYPE_PLATFORM_APP,
  };
  // Special case: no items are 'STATUS_ACTIVE'.
  SetupMockShelfModel(types, arraysize(types), -1);

  // If there are no active status, pick the first running item as a fallback.
  EXPECT_EQ(2, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));
  EXPECT_EQ(2, GetNextActivatedItemIndex(model(), CYCLE_BACKWARD));
}

}  // namespace ash
