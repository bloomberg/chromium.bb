// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/wrench_menu_model.h"

#include "base/logging.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/menu_model_test.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

class WrenchMenuModelTest : public BrowserWithTestWindowTest,
                            public MenuModelTest {
};

TEST_F(WrenchMenuModelTest, Basics) {
  WrenchMenuModel model(&delegate_, browser());
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
  EXPECT_EQ(delegate_.execute_count_, 2);
  EXPECT_EQ(delegate_.enable_count_, 2);

  delegate_.execute_count_ = 0;
  delegate_.enable_count_ = 0;

  // Choose something from the tools submenu and make sure it makes it back to
  // the delegate as well. Use the first submenu as the tools one.
  int toolsModelIndex = -1;
  for (int i = 0; i < itemCount; ++i) {
    if (model.GetTypeAt(i) == menus::MenuModel::TYPE_SUBMENU) {
      toolsModelIndex = i;
      break;
    }
  }
  EXPECT_GT(toolsModelIndex, -1);
  menus::MenuModel* toolsModel = model.GetSubmenuModelAt(toolsModelIndex);
  EXPECT_TRUE(toolsModel);
  EXPECT_GT(toolsModel->GetItemCount(), 2);
  toolsModel->ActivatedAt(2);
  EXPECT_TRUE(toolsModel->IsEnabledAt(2));
  EXPECT_EQ(delegate_.execute_count_, 1);
  EXPECT_EQ(delegate_.enable_count_, 1);
}

class EncodingMenuModelTest : public BrowserWithTestWindowTest,
                              public MenuModelTest {
};

TEST_F(EncodingMenuModelTest, IsCommandIdCheckedWithNoTabs) {
  EncodingMenuModel model(browser());
  ASSERT_EQ(NULL, browser()->GetSelectedTabContents());
  EXPECT_FALSE(model.IsCommandIdChecked(IDC_ENCODING_ISO88591));
}
