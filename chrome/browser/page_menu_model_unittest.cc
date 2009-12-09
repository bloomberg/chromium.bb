// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/page_menu_model.h"

#include "base/logging.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"

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

class PageMenuModelTest : public BrowserWithTestWindowTest {
 public:
};

TEST_F(PageMenuModelTest, Basics) {
  Delegate delegate;
  PageMenuModel model(&delegate, browser());

  // Verify it has items. The number varies by platform, so we don't check
  // the exact number.
  EXPECT_GT(model.GetItemCount(), 10);

  // Execute a couple of the items and make sure it gets back to our delegate.
  model.ActivatedAt(0);
  EXPECT_TRUE(model.IsEnabledAt(0));
  model.ActivatedAt(3);
  EXPECT_TRUE(model.IsEnabledAt(3));
  EXPECT_EQ(delegate.execute_count_, 2);
  EXPECT_EQ(delegate.enable_count_, 2);

  delegate.execute_count_ = 0;
  delegate.enable_count_ = 0;

  // Choose something from the zoom submenu and make sure it makes it back to
  // the delegate as well.
  menus::MenuModel* zoomModel = model.GetSubmenuModelAt(10);
  EXPECT_TRUE(zoomModel);
  EXPECT_GT(zoomModel->GetItemCount(), 1);
  zoomModel->ActivatedAt(1);
  EXPECT_TRUE(zoomModel->IsEnabledAt(1));
  EXPECT_EQ(delegate.execute_count_, 1);
  EXPECT_EQ(delegate.enable_count_, 1);
}
