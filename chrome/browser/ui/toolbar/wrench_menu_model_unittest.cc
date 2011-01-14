// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/wrench_menu_model.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/menu_model_test.h"
#include "chrome/test/testing_profile.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

class WrenchMenuModelTest : public BrowserWithTestWindowTest,
                            public ui::AcceleratorProvider {
 public:
  // Don't handle accelerators.
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) { return false; }
};

// Copies parts of MenuModelTest::Delegate and combines them with the
// WrenchMenuModel since WrenchMenuModel is now a SimpleMenuModel::Delegate and
// not derived from SimpleMenuModel.
class TestWrenchMenuModel : public WrenchMenuModel {
 public:
  TestWrenchMenuModel(ui::AcceleratorProvider* provider,
                      Browser* browser)
      : WrenchMenuModel(provider, browser),
        execute_count_(0),
        checked_count_(0),
        enable_count_(0) {
  }

  // Testing overrides to ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const {
    bool val = WrenchMenuModel::IsCommandIdChecked(command_id);
    if (val)
      checked_count_++;
    return val;
  }

  virtual bool IsCommandIdEnabled(int command_id) const {
    ++enable_count_;
    return true;
  }

  virtual void ExecuteCommand(int command_id) { ++execute_count_; }

  int execute_count_;
  mutable int checked_count_;
  mutable int enable_count_;
};

TEST_F(WrenchMenuModelTest, Basics) {
  TestWrenchMenuModel model(this, browser());
  int itemCount = model.GetItemCount();

  // Verify it has items. The number varies by platform, so we don't check
  // the exact number.
  EXPECT_GT(itemCount, 10);

  // Execute a couple of the items and make sure it gets back to our delegate.
  // We can't use CountEnabledExecutable() here because the encoding menu's
  // delegate is internal, it doesn't use the one we pass in.
  model.ActivatedAt(0);
  EXPECT_TRUE(model.IsEnabledAt(0));
  // Make sure to use the index that is not separator in all configurations.
  model.ActivatedAt(2);
  EXPECT_TRUE(model.IsEnabledAt(2));
  EXPECT_EQ(model.execute_count_, 2);
  EXPECT_EQ(model.enable_count_, 2);

  model.execute_count_ = 0;
  model.enable_count_ = 0;

  // Choose something from the tools submenu and make sure it makes it back to
  // the delegate as well. Use the first submenu as the tools one.
  int toolsModelIndex = -1;
  for (int i = 0; i < itemCount; ++i) {
    if (model.GetTypeAt(i) == ui::MenuModel::TYPE_SUBMENU) {
      toolsModelIndex = i;
      break;
    }
  }
  EXPECT_GT(toolsModelIndex, -1);
  ui::MenuModel* toolsModel = model.GetSubmenuModelAt(toolsModelIndex);
  EXPECT_TRUE(toolsModel);
  EXPECT_GT(toolsModel->GetItemCount(), 2);
  toolsModel->ActivatedAt(2);
  EXPECT_TRUE(toolsModel->IsEnabledAt(2));
  EXPECT_EQ(model.execute_count_, 1);
  EXPECT_EQ(model.enable_count_, 1);
}

class EncodingMenuModelTest : public BrowserWithTestWindowTest,
                              public MenuModelTest {
};

TEST_F(EncodingMenuModelTest, IsCommandIdCheckedWithNoTabs) {
  EncodingMenuModel model(browser());
  ASSERT_EQ(NULL, browser()->GetSelectedTabContents());
  EXPECT_FALSE(model.IsCommandIdChecked(IDC_ENCODING_ISO88591));
}
