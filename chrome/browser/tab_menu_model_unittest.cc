// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/tab_menu_model.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

// A menu delegate that counts the number of times certain things are called
// to make sure things are hooked up properly.
class Delegate : public menus::SimpleMenuModel::Delegate {
 public:
  Delegate() : execute_count_(0), enable_count_(0) { }

  virtual bool IsCommandIdChecked(int command_id) const { return false; }
  virtual bool IsCommandIdEnabled(int command_id) const {
    ++enable_count_;
    return true;
  }
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      menus::Accelerator* accelerator) { return false; }
  virtual void ExecuteCommand(int command_id) { ++execute_count_; }

  int execute_count_;
  mutable int enable_count_;
};

class TabMenuModelTest : public PlatformTest {
};

// Recursively checks the enabled state and executes a command on every item
// that's not a separator or a submenu parent item. The returned count should
// match the number of times the delegate is called to ensure every item works.
static void CountEnabledExecutable(menus::MenuModel* model, int* count) {
  for (int i = 0; i < model->GetItemCount(); ++i) {
    menus::MenuModel::ItemType type = model->GetTypeAt(i);
    switch (type) {
      case menus::MenuModel::TYPE_SEPARATOR:
        continue;
      case menus::MenuModel::TYPE_SUBMENU:
        CountEnabledExecutable(model->GetSubmenuModelAt(i), count);
        break;
      default:
        model->IsEnabledAt(i);  // Check if it's enabled (ignore answer).
        model->ActivatedAt(i);  // Execute it.
        (*count)++;  // Increment the count of executable items seen.
        break;
    }
  }
}

TEST_F(TabMenuModelTest, Basics) {
  Delegate delegate;
  TabMenuModel model(&delegate);

  // Verify it has items. The number varies by platform, so we don't check
  // the exact number.
  EXPECT_GT(model.GetItemCount(), 5);

  int item_count = 0;
  CountEnabledExecutable(&model, &item_count);
  EXPECT_GT(item_count, 0);
  EXPECT_EQ(item_count, delegate.execute_count_);
  EXPECT_EQ(item_count, delegate.enable_count_);
}
