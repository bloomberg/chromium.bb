// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_navigator.h"

#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_types.h"
#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

class LauncherNavigatorTest : public testing::Test {
 public:
  LauncherNavigatorTest() {}

 protected:
  virtual void SetUp() {
    model_.reset(new LauncherModel);
  }

  void SetupMockLauncherModel(LauncherItemType* types,
                              int types_length,
                              int focused_index) {
    for (int i = 0; i < types_length; ++i) {
      LauncherItem new_item;
      new_item.type = types[i];
      new_item.status =
          (types[i] == TYPE_TABBED) ? STATUS_RUNNING : STATUS_CLOSED;
      model_->Add(new_item);
    }

    // Set the focused item.
    if (focused_index >= 0) {
      LauncherItem focused_item =model_->items()[focused_index];
      if (focused_item.type == TYPE_TABBED) {
        focused_item.status = STATUS_ACTIVE;
        model_->Set(focused_index, focused_item);
      }
    }
  }

  const LauncherModel& model() { return *model_.get(); }

 private:
  scoped_ptr<LauncherModel> model_;

  DISALLOW_COPY_AND_ASSIGN(LauncherNavigatorTest);
};

} // namespace

TEST_F(LauncherNavigatorTest, BasicCycle) {
  // An app shortcut and three windows
  LauncherItemType types[] = {
    TYPE_APP_SHORTCUT, TYPE_TABBED, TYPE_TABBED, TYPE_TABBED,
  };
  // LauncherModel automatically adds BROWSER_SHORTCUT item at the
  // beginning, so '2' refers the first TYPE_TABBED item.
  SetupMockLauncherModel(types, arraysize(types), 2);

  EXPECT_EQ(3, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));

  // Fourth one.  It will skip the APP_SHORTCUT at the beginning of the list and
  // the APP_LIST item which is automatically added at the end of items.
  EXPECT_EQ(4, GetNextActivatedItemIndex(model(), CYCLE_BACKWARD));
}

TEST_F(LauncherNavigatorTest, WrapToBeginning) {
  LauncherItemType types[] = {
    TYPE_APP_SHORTCUT, TYPE_TABBED, TYPE_TABBED, TYPE_TABBED,
  };
  SetupMockLauncherModel(types, arraysize(types), 4);

  // Second one.  It skips the APP_LIST item at the end of the list,
  // wraps to the beginning, and skips BROWSER_SHORTCUT and APP_SHORTCUT
  // at the beginning of the list.
  EXPECT_EQ(2, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));
}

TEST_F(LauncherNavigatorTest, Empty) {
  SetupMockLauncherModel(NULL, 0, -1);
  EXPECT_EQ(-1, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));
  EXPECT_EQ(-1, GetNextActivatedItemIndex(model(), CYCLE_BACKWARD));
}

TEST_F(LauncherNavigatorTest, SingleEntry) {
  LauncherItemType type = TYPE_TABBED;
  SetupMockLauncherModel(&type, 1, 1);

  // If there's only one item there and it is already active, there's no item
  // to be activated next.
  EXPECT_EQ(-1, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));
  EXPECT_EQ(-1, GetNextActivatedItemIndex(model(), CYCLE_BACKWARD));
}

TEST_F(LauncherNavigatorTest, NoActive) {
  LauncherItemType types[] = {
    TYPE_TABBED, TYPE_TABBED,
  };
  // Special case: no items are 'STATUS_ACTIVE'.
  SetupMockLauncherModel(types, arraysize(types), -1);

  // If there are no active status, pick the first running item as a fallback.
  EXPECT_EQ(1, GetNextActivatedItemIndex(model(), CYCLE_FORWARD));
  EXPECT_EQ(1, GetNextActivatedItemIndex(model(), CYCLE_BACKWARD));
}

}  // namespace ash
