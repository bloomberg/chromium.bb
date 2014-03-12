// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_helper.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

// Tests for AshTestHelper. Who will watch the watchers? And who will test
// the tests?
class AshTestHelperTest : public testing::Test {
 public:
  AshTestHelperTest() {}
  virtual ~AshTestHelperTest() {}

  virtual void SetUp() OVERRIDE {
    testing::Test::SetUp();
    ash_test_helper_.reset(new ash::test::AshTestHelper(&message_loop_));
    ash_test_helper_->SetUp(true);
  }

  virtual void TearDown() OVERRIDE {
    ash_test_helper_->TearDown();
    testing::Test::TearDown();
  }

  ash::test::AshTestHelper* ash_test_helper() {
    return ash_test_helper_.get();
  }

 private:
  base::MessageLoopForUI message_loop_;
  scoped_ptr<ash::test::AshTestHelper> ash_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(AshTestHelperTest);
};

// Ensure that we have initialized enough of Ash to create and show a window.
TEST_F(AshTestHelperTest, AshTestHelper) {
  // Check initial state.
  EXPECT_TRUE(ash_test_helper()->message_loop());
  EXPECT_TRUE(ash_test_helper()->test_shell_delegate());
  EXPECT_TRUE(ash_test_helper()->CurrentContext());

  // Enough state is initialized to create a window.
  using views::Widget;
  scoped_ptr<Widget> w1(new Widget);
  Widget::InitParams params;
  params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.context = ash_test_helper()->CurrentContext();
  w1->Init(params);
  w1->Show();
  EXPECT_TRUE(w1->IsActive());
  EXPECT_TRUE(w1->IsVisible());
}
