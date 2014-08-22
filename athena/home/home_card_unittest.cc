// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/public/home_card.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/test/athena_test_base.h"
#include "athena/wm/public/window_manager.h"
#include "ui/aura/window.h"
#include "ui/events/test/event_generator.h"

namespace athena {

typedef test::AthenaTestBase HomeCardTest;

TEST_F(HomeCardTest, BasicTransition) {
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());

  WindowManager::GetInstance()->ToggleOverview();
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());

  WindowManager::GetInstance()->ToggleOverview();
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());
}

TEST_F(HomeCardTest, VirtualKeyboardTransition) {
  // Minimized -> Hidden for virtual keyboard.
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());
  const gfx::Rect vk_bounds(0, 0, 100, 100);
  HomeCard::Get()->UpdateVirtualKeyboardBounds(vk_bounds);
  EXPECT_EQ(HomeCard::HIDDEN, HomeCard::Get()->GetState());
  HomeCard::Get()->UpdateVirtualKeyboardBounds(gfx::Rect());
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());

  // bottom -> centered for virtual keyboard.
  WindowManager::GetInstance()->ToggleOverview();
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());
  HomeCard::Get()->UpdateVirtualKeyboardBounds(vk_bounds);
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, HomeCard::Get()->GetState());
  HomeCard::Get()->UpdateVirtualKeyboardBounds(gfx::Rect());
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());

  // Overview mode has to finish before ending test, otherwise it crashes.
  // TODO(mukai): fix this.
  WindowManager::GetInstance()->ToggleOverview();
}

// Verify if the home card is correctly minimized after app launch.
TEST_F(HomeCardTest, AppSelection) {
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());

  WindowManager::GetInstance()->ToggleOverview();
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());

  athena::ActivityManager::Get()->AddActivity(
      athena::ActivityFactory::Get()->CreateWebActivity(
          NULL, GURL("http://www.google.com/")));
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());
}

TEST_F(HomeCardTest, Accelerators) {
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());

  ui::test::EventGenerator generator(root_window());
  generator.PressKey(ui::VKEY_L, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, HomeCard::Get()->GetState());

  generator.PressKey(ui::VKEY_L, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());

  // Do nothing for BOTTOM.
  WindowManager::GetInstance()->ToggleOverview();
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());
  generator.PressKey(ui::VKEY_L, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());

  // Do nothing if the centered state is a temporary state.
  HomeCard::Get()->UpdateVirtualKeyboardBounds(gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, HomeCard::Get()->GetState());
  generator.PressKey(ui::VKEY_L, ui::EF_CONTROL_DOWN);
  EXPECT_EQ(HomeCard::VISIBLE_CENTERED, HomeCard::Get()->GetState());
}

TEST_F(HomeCardTest, MouseClick) {
  ASSERT_EQ(HomeCard::VISIBLE_MINIMIZED, HomeCard::Get()->GetState());

  // Mouse click at the bottom of the screen should invokes overview mode and
  // changes the state to BOTTOM.
  ui::test::EventGenerator generator(root_window());
  gfx::Rect screen_rect(root_window()->bounds());
  generator.MoveMouseTo(gfx::Point(
      screen_rect.x() + screen_rect.width() / 2, screen_rect.bottom() - 1));
  generator.ClickLeftButton();

  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());
  EXPECT_TRUE(WindowManager::GetInstance()->IsOverviewModeActive());

  // Further clicks are simply ignored.
  generator.ClickLeftButton();
  EXPECT_EQ(HomeCard::VISIBLE_BOTTOM, HomeCard::Get()->GetState());
  EXPECT_TRUE(WindowManager::GetInstance()->IsOverviewModeActive());
}

}  // namespace athena
