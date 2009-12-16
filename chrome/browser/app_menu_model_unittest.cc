// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/app_menu_model.h"

#include "base/logging.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/menu_model_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class AppMenuModelTest : public BrowserWithTestWindowTest,
                         public MenuModelTest {
};

TEST_F(AppMenuModelTest, Basics) {
  AppMenuModel model(&delegate_, browser());

  // Verify it has items. The number varies by platform, so we don't check
  // the exact number.
  EXPECT_GT(model.GetItemCount(), 5);

  int item_count = 0;
  CountEnabledExecutable(&model, &item_count);
  EXPECT_GT(item_count, 0);
  EXPECT_EQ(item_count, delegate_.execute_count_);
  EXPECT_EQ(item_count, delegate_.enable_count_);
}
