// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/screen_mus.h"

#include "ash/mus/window_manager.h"
#include "ash/mus/window_manager_application.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ui/aura/test/test_window_delegate.h"

namespace ash {
namespace test {

class ScreenMusTest : public AshTestBase {
 public:
  ScreenMusTest() {}
  ~ScreenMusTest() override {}

  display::Screen* screen_mus() {
    return ash_test_helper()->window_manager_app()->window_manager()->screen();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenMusTest);
};

TEST_F(ScreenMusTest, GetWindowAtScreenPoint) {
  UpdateDisplay("1024x600");

  aura::test::TestWindowDelegate test_delegate;
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &test_delegate, 23, gfx::Rect(20, 20, 50, 50)));

  aura::Window* at_point =
      screen_mus()->GetWindowAtScreenPoint(gfx::Point(25, 25));
  EXPECT_EQ(w1.get(), at_point);

  // Create a second window which overlaps with w1.
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &test_delegate, 17, gfx::Rect(25, 25, 50, 50)));
  at_point = screen_mus()->GetWindowAtScreenPoint(gfx::Point(25, 25));
  EXPECT_EQ(w2.get(), at_point);

  // Raise w1.
  w1->parent()->StackChildAtTop(w1.get());
  at_point = screen_mus()->GetWindowAtScreenPoint(gfx::Point(25, 25));
  EXPECT_EQ(w1.get(), at_point);
}

}  // namespace test
}  // namespace ash
