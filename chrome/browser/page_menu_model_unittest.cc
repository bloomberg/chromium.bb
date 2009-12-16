// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/page_menu_model.h"

#include "base/logging.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/menu_model_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class PageMenuModelTest : public BrowserWithTestWindowTest,
                          public MenuModelTest {
};

TEST_F(PageMenuModelTest, Basics) {
  PageMenuModel model(&delegate_, browser());

  // Verify it has items. The number varies by platform, so we don't check
  // the exact number.
  EXPECT_GT(model.GetItemCount(), 10);

  // Execute a couple of the items and make sure it gets back to our delegate.
  // We can't use CountEnabledExecutable() here because the encoding menu's
  // delegate is internal, it doesn't use the one we pass in.
  model.ActivatedAt(0);
  EXPECT_TRUE(model.IsEnabledAt(0));
  model.ActivatedAt(3);
  EXPECT_TRUE(model.IsEnabledAt(3));
  EXPECT_EQ(delegate_.execute_count_, 2);
  EXPECT_EQ(delegate_.enable_count_, 2);

  delegate_.execute_count_ = 0;
  delegate_.enable_count_ = 0;

  // Choose something from the zoom submenu and make sure it makes it back to
  // the delegate as well.
  menus::MenuModel* zoomModel = model.GetSubmenuModelAt(10);
  EXPECT_TRUE(zoomModel);
  EXPECT_GT(zoomModel->GetItemCount(), 1);
  zoomModel->ActivatedAt(1);
  EXPECT_TRUE(zoomModel->IsEnabledAt(1));
  EXPECT_EQ(delegate_.execute_count_, 1);
  EXPECT_EQ(delegate_.enable_count_, 1);
}
